"""External NSB-7 SMS ingress save/load: exact architecture/replay/storage, no speech."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsb7_state_check import check_architecture
from tools.nsb7_pin_services_check import check_source,check_authentication
from tools.nsb7_sms_check import check_read,check_read_frame
from tools.nsb7_host_outgoing_call_check import check_clock
from tools.nsb7_radio_check import contract
from tools.nsb7_storage_check import inspect
from tools.nsb7_pin_check import check_frame
from tools.radio_sms_acceptance_common import FIRST_SMS_DELIVER_BODY,SMS_NVRAM_OFFSET
from tools.run_sip_handset_gate import verify_handset_health
from tools.radio_call_lifecycle_common import require_ordered

STARTUP_FRAME='2cf67eee948c37823877d4e08fb96ee35836f6618413d937df2cdbed656fc9ab'


def check(text,initial,final):
    check_source(initial)
    check_authentication(text.split('nsb7_state: event=saved',1)[0])
    check_architecture(text,'sms',('TX packet','RX enqueue','GSM service','sim_device:',
                                    'ccont_rtc:','ccont_analog:'))
    if re.findall(r'incoming sms state id=1 epoch=(\d+) phase=(\w+)',text)!=[
            ('1','queued'),('1','delivered'),('2','queued'),('2','delivered')]:
        raise ValueError('external SMS reference/restored epoch lifecycle differs')
    if len(re.findall(r'incoming sms id=\d+ result=accepted',text))!=1:
        raise ValueError('external SMS was admitted more than once')
    triggers=re.findall(r'nsb7_host_sms_save_trigger: pc=002645ec pd=09 message=01 t=([0-9.]+) observer_writes=0',text)
    saves=re.findall(r'nsb7_state: event=saved .*? t=([0-9.]+)',text)
    if len(triggers)!=1 or len(saves)!=1 or not 0<=float(saves[0])-float(triggers[0])<=0.00011:
        raise ValueError('save was not bound to observed CP-DATA ingress')
    require_ordered(text,[
        ('admission',r'incoming sms id=1 result=accepted'),
        ('CP-DATA',r'GSM service downlink kind=16 sapi=3 pd=09 message=01'),
        ('trigger',r'nsb7_host_sms_save_trigger:'),('save',r'nsb7_state: event=saved'),
        ('reference delivery',r'incoming sms state id=1 epoch=1 phase=delivered'),
        ('load',r'nsb7_state: event=restored'),
        ('restored queued',r'incoming sms state id=1 epoch=2 phase=queued'),
        ('restored delivered',r'incoming sms state id=1 epoch=2 phase=delivered'),
        ('physical reading',r'nsb7_sms_input: key=Menu state=1 t=26\.'),
        ('complete',r'nsb7_state_sms: complete'),
    ],'external incoming SMS restore')
    before=text.split('nsb7_state: event=saved',1)[0]
    if 'GSM service uplink sapi=3 pd=09 message=' in before or 'sim_device: update fid=6f3c' in before:
        raise ValueError('SMS was already acknowledged/stored before ingress save')
    for phase in ('reference','restored'):
        interval=text.split(f'state_replay: phase={phase} event=begin',1)[1].split(f'state_replay: phase={phase} event=end',1)[0]
        if (interval.count('GSM service uplink sapi=3 pd=09 message=04 ')!=1 or
                interval.count('GSM service uplink sapi=3 pd=09 message=01 length=5 data=8901020240')!=1 or
                interval.count('sim_device: update fid=6f3c record=1 length=176')!=1):
            raise ValueError('each replay branch must acknowledge and store exactly one SMS')
    # The full branches were compared above; only the restored branch reaches
    # final storage. Do not count the discarded reference execution twice.
    continuous=text.split('state_replay: phase=reference event=begin',1)[0]+text.split('state_replay: phase=restored event=begin',1)[1]
    delivered=bytearray(initial)
    record=b'\x03'+FIRST_SMS_DELIVER_BODY
    delivered[SMS_NVRAM_OFFSET:SMS_NVRAM_OFFSET+176]=record.ljust(176,b'\xff')
    check_read(continuous,final,bytes(delivered),read_times=(26,28,30,32))
    # This shorter SMS fixture observes startup control00, not the later0f
    # write required by the longer call lifecycle; RTC continuity is unchanged.
    check_clock(continuous,finish=35,control_values=('00',))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image',type=Path)
    parser.add_argument('run',type=Path)
    parser.add_argument('--console',type=Path,required=True)
    args=parser.parse_args()
    try:
        contract(args.image.read_bytes())
        verify_handset_health(args.console.read_text())
        storage=args.run/'nvram/nsb7hle'
        inspect((storage/'flash').read_bytes())
        check((args.run/'error.log').read_text(),(args.run/'initial_sim_card.bin').read_bytes(),
              (storage/'sim_card').read_bytes())
        check_read_frame(args.run/'snap/nsb7_sms_read_32.png')
        if not (args.run/'sta/nsb7hle/nsb7_sms.sta').is_file():raise ValueError('actual snapshot absent')
        for phase in ('reference','restored'):
            check_frame(args.run/f'snap/nsb7_state_sms_{phase}.png',STARTUP_FRAME)
    except (OSError,ValueError,RuntimeError,IndexError) as error:
        parser.exit(1,f'NSB-7 incoming host SMS restore: FAIL: {error}\n')
    print('PASS: external SMS admitted once, exact ingress architecture/replay, epoch-safe delivery and physical read; speech_tested=0')


if __name__=='__main__':main()
