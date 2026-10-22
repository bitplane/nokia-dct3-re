"""NSB-7 physical rail-off, retained RTC and authenticated restart."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsb7_pin_services_check import check_authentication,check_source
from tools.nsb7_pin_registration_check import request
from tools.nsb7_radio_check import PACKET,check_location_update,contract
from tools.nsb7_sms_check import tx_bytes
from tools.nsb7_host_outgoing_call_check import check_clock
from tools.nsb7_pin_check import check_frame
from tools.nsb7_storage_check import inspect
from tools.power_domain_contract import require_endpoint_silence
from tools.radio_call_lifecycle_common import require_ordered

FRAMES={
    'nsb7_power_ready.png':'32c41625e02a87a0ec97ec98a250040fc87ad8c5722c78200c54b27f7de2d560',
    'nsb7_power_off.png':'5fdab89cdada4b6ac821d32facb589dfb541e9856fe3b569e8da49646fc3d249',
    'nsb7_power_pin.png':'a12b3c3fdb4a3d1e8ffeb5fc8986c3bc008bb86b1f2e927440848fe0420ffc0c',
    'nsb7_power_security.png':'24cbab0ad5b13376772a44f0eec7835a7ea865795c9ee229c7631fa2e0ea1fb2',
    'nsb7_power_menu.png':'76728306449ad0f2ae8cbdbd1fc5e880d3dcff054c955e8b4a2aabc87db8e7e4',
    'nsb7_power_final.png':'29dcc2200f6f5c170afea80e751d1d3afefd83332964c4bdc0f2a843be5e2567',
}


def check(text):
    if '[LUA ERROR]' in text or 'invalid_extent' in text:
        raise ValueError('power observer failed')
    offs=list(re.finditer(r'ccont_power: event=off t=([0-9.]+)',text))
    wakes=list(re.finditer(r'ccont_power: event=wake cause=(\w+) t=([0-9.]+)',text))
    if len(offs)!=1 or len(wakes)!=1 or wakes[0][1]!='02':
        raise ValueError('expected one rail-off and PWRONX wake')
    off,wake=offs[0],wakes[0]
    if float(wake[2])-float(off[1])<10:
        raise ValueError('retained off interval too short')
    check_authentication(text[:off.start()])
    interval=text[off.end():wake.start()]
    require_endpoint_silence(interval,'NSB-7 powered endpoint activity while off')
    ticks=re.findall(r'ccont_rtc: event=second [^\n]*t=([0-9.]+)',interval)
    if len(ticks)<10 or any(not float(off[1])<float(time)<float(wake[2]) for time in ticks):
        raise ValueError('retained RTC ticks absent from the rail-off interval')
    check_clock(text,finish=101,control_values=('00',))
    steps=[('Power','1','45.000000000'),('Power','0','49.000000000'),
           ('Power','1','65.000000000'),('Power','0','67.000000000')]
    for start,digits in ((73,'1234'),(81,'12345')):
        for time,key in [(start+i,'Keypad '+d) for i,d in enumerate(digits)]+[(start+len(digits),'Left Softkey / Menu')]:
            steps.extend((key,state,f'{time}.{fraction}') for state,fraction in
                         (('1','000000000'),('0','200000000')))
    for time,key in ((95,'Left Softkey / Menu'),(98,'End')):
        steps.extend((key,state,f'{time}.{fraction}') for state,fraction in
                     (('1','000000000'),('0','200000000')))
    rows=list(re.finditer(r'nsb7_power_input: key=(.*?) state=([01]) t=([0-9.]+)',text))
    if [row.groups() for row in rows]!=steps:
        raise ValueError('physical power/authentication/menu sequence differs')
    for index in range(4,len(rows),2):
        key=rows[index][1]
        code='19' if key=='Left Softkey / Menu' else '0f' if key=='End' else f'{int(key[-1]):02x}'
        if f'nsb7_key_decode: key={code} observer_writes=0' not in text[rows[index].end():rows[index+1].start()]:
            raise ValueError('restart physical contact did not decode')
    require_ordered(text,(
        ('initial ready',r'nsb7_power: ready t=44\.000000000'),
        ('off press',r'nsb7_power_input: key=Power state=1 t=45\.'),
        ('off release',r'nsb7_power_input: key=Power state=0 t=49\.'),
        ('rails off',r'ccont_power: event=off t='),
        ('observer cursor only',r'nsb7_radio_capture: reset_cursor t=55\.000000000 observer_writes=0'),
        ('wake press',r'nsb7_power_input: key=Power state=1 t=65\.'),
        ('PWRONX wake',r'ccont_power: event=wake cause=02'),
        ('wake release',r'nsb7_power_input: key=Power state=0 t=67\.'),
        ('PIN submitted',r'nsb7_power_input: key=Left Softkey / Menu state=1 t=77\.'),
        ('PIN accepted',r'SIM status ins=20 sw=9000'),
        ('retained-location registration','GSM service establish sapi=0 pd=05 message=08 length=18 data='+request(True).hex()),
        ('location accepted',r'LAPDm Location Updating Accept acknowledged'),
        ('phone code submitted',r'nsb7_power_input: key=Left Softkey / Menu state=1 t=86\.'),
        ('phone code accepted',r'nsb7_security_retry: match=1 retries=00'),
        ('Menu',r'nsb7_power_input: key=Left Softkey / Menu state=1 t=95\.'),
        ('complete',r'nsb7_power: done t=101\.000000000'),
    ),'NSB-7 physical power lifecycle')
    after=text[wake.end():]
    cause=re.search(r'ccont_power: event=cause_read data=([0-9a-f]+)',after)
    if not cause or int(cause[1],16)&7!=3:
        raise ValueError('wake cause must retain ready/PWRONX without charger')
    wire=tx_bytes(after)
    if wire.count(bytes.fromhex('a02000010831323334ffffffff'))!=1 or b'\xa0\xd6' in wire:
        raise ValueError('restart PIN/retained-location SIM exchange differs')
    packets=PACKET.findall(after)
    if any(row[1]=='00' and row[2]=='0' for row in packets):
        raise ValueError('observer decoded reset-cleared ring words as packets')
    requests=[row for row in packets if row[1]=='1b' and row[3].startswith('0080013f49')]
    if len(requests)!=1: raise ValueError('restart must publish one Location Updating request')
    first_ack=int(requests[0][0])+1
    # Boot initializes the old context address before this new transaction.
    transaction=after[after.index(f'nsb7_radio_tx: index={first_ack-1} '):]
    check_location_update(transaction.split(f'nsb7_radio_tx: index={first_ack+3} ',1)[0],
                          first_ack=first_ack,request=request(True))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image',type=Path)
    parser.add_argument('run',type=Path)
    args=parser.parse_args()
    try:
        contract(args.image.read_bytes())
        check((args.run/'error.log').read_text())
        storage=args.run/'nvram/nsb7hle'
        initial=(args.run/'initial_sim_card.bin').read_bytes()
        check_source(initial)
        if (storage/'sim_card').read_bytes()!=initial:
            raise ValueError('power restart changed complete retained card')
        inspect((storage/'flash').read_bytes())
        for name,digest in FRAMES.items(): check_frame(args.run/'snap'/name,digest)
    except (OSError,ValueError) as error:
        parser.exit(1,f'NSB-7 power lifecycle: FAIL: {error}\n')
    print('PASS: physical rail-off/PWRONX, retained RTC/card, fresh PIN/phone code, warm registration and menu recovery')


if __name__=='__main__': main()
