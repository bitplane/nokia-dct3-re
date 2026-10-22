"""Authenticated NSB-7 host SMS failure, exact storage and physical recovery."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsb7_outgoing_sms_check import check_inputs
from tools.nsb7_pin_services_check import check_authentication,check_sms_storage
from tools.radio_outgoing_host_sms_trace_check import verify as check_host
from tools.radio_call_lifecycle_common import require_ordered
from tools.nsb7_host_outgoing_call_check import check_clock
from tools.nsb7_pin_check import check_frame
from tools.nsb7_sms_check import check_read_frame
from tools.nsb7_radio_check import contract
from tools.nsb7_storage_check import inspect


def check(text,*,silent=False):
    check_authentication(text.split('nsb7_sms_input:',1)[0])
    check_inputs(text,delay=4)
    check_host(text,octets=1,outcome='rp_silence' if silent else 'rp_error')
    submit=('gsm_sms_submit: cp=39 rp=01 smsc=1234567890 destination=5551234 '
            'alphabet=0 user_length=1 outcome=0 status_report=0')
    if text.count('gsm_sms_submit:')!=1 or submit not in text or \
            'data=390118000100069121436587090d11000781551532f40000a70141' not in text:
        raise ValueError('failed Reply must retain exactly one A/5551234 submission')
    if text.count('GSM service uplink sapi=3 pd=09 message=01 length=5 data=8901020240')!=1:
        raise ValueError('incoming message completion differs')
    code='3' if silent else '1'
    if re.findall(r'gsm_call_adapter: sms decision id=1 outcome=(\d+) result=accepted',text)!=[code]:
        raise ValueError('SMS must accept only its one matching host decision')
    if re.findall(r'gsm_call_adapter: sms state id=1 epoch=(\d+) phase=(\w+)',text)!=[
            ('1','rejected'),('1','ended')]:
        raise ValueError('failed SMS host phase/epoch sequence differs')
    delay=90 if silent else 0
    expected=[(key,state,f'{time+delay}.{fraction}')
              for time,key in ((78,'End'),(82,'Left Softkey / Menu'),(85,'End'))
              for state,fraction in (('1','000000000'),('0','200000000'))]
    rows=list(re.finditer(r'nsb7_sms_recovery: key=(.*?) state=([01]) t=([0-9.]+)',text))
    if [row.groups() for row in rows]!=expected:
        raise ValueError('SMS recovery physical sequence differs')
    for prefix in ('nsb7_sms_reply_input','nsb7_sms_recovery'):
        presses=list(re.finditer(rf'{prefix}: key=(.*?) state=([01]) t=([0-9.]+)',text))
        for index in range(0,len(presses),2):
            key=presses[index][1]
            code={'End':'0f','Left Softkey / Menu':'19','Scroll Down':'18','Keypad 2':'02'}[key]
            if f'nsb7_key_decode: key={code} observer_writes=0' not in text[presses[index].end():presses[index+1].start()]:
                raise ValueError('SMS physical contact did not decode')
    require_ordered(text,(
        ('host ended',r'gsm_call_adapter: sms state id=1 epoch=1 phase=ended'),
        ('physical End',rf'nsb7_sms_recovery: key=End state=1 t={78+delay}\.'),
        ('Menu',rf'nsb7_sms_recovery: key=Left Softkey / Menu state=1 t={82+delay}\.'),
        ('final End',rf'nsb7_sms_recovery: key=End state=1 t={85+delay}\.'),
        ('finished',rf'nsb7_sms_recovery: done t={88+delay}\.000000000'),
    ),'NSB-7 SMS failure recovery')
    if 'GSM outgoing request ' in text or '[LUA ERROR]' in text:
        raise ValueError('unexpected call or failed physical harness')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image',type=Path)
    parser.add_argument('run',type=Path)
    parser.add_argument('--rp-silence',action='store_true')
    args=parser.parse_args()
    try:
        contract(args.image.read_bytes())
        text=(args.run/'error.log').read_text()
        check(text,silent=args.rp_silence)
        check_clock(text,finish=178 if args.rp_silence else 88,control_values=('00',))
        storage=args.run/'nvram/nsb7hle'
        check_sms_storage(text,(args.run/'initial_sim_card.bin').read_bytes(),
                          (storage/'sim_card').read_bytes(),update_reference=False)
        inspect((storage/'flash').read_bytes())
        check_read_frame(args.run/'snap/nsb7_sms_read_29.png')
        if args.rp_silence:
            failure,digest='nsb7_sms_pending_105.png','775f99ae5ebae196c3e1a45af2d2c6e637797cec19bcb8ad80daadec251e96fa'
            menu,final='nsb7_sms_recovery_172.png','9da919d7d031f0e7d2f8184dab4a20d2f91d747ac025de9908bf24112faa162a'
        else:
            failure,digest='nsb7_sms_reply_final_48.3.png','cd33c2decd7f4bcb6493e618c14000adc293542f178aaf8f552c39aa8e09cc24'
            menu,final='nsb7_sms_recovery_82.png','29dcc2200f6f5c170afea80e751d1d3afefd83332964c4bdc0f2a843be5e2567'
        check_frame(args.run/'snap'/failure,digest)
        check_frame(args.run/'snap'/menu,'76728306449ad0f2ae8cbdbd1fc5e880d3dcff054c955e8b4a2aabc87db8e7e4')
        check_frame(args.run/'snap/nsb7_sms_recovery_final.png',final)
    except (OSError,ValueError) as error:
        parser.exit(1,f'NSB-7 failed host SMS: FAIL: {error}\n')
    print('PASS: host SMS '+('silence' if args.rp_silence else 'rejection')+
          ', firmware closure, no EF_SMSS update, exact UI/card and physical recovery')


if __name__=='__main__': main()
