"""Qualify an external USSD notification, physical reading and unchanged card."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsb7_pin_services_check import check_source,check_authentication
from tools.nsb7_pin_check import check_frame
from tools.nsb7_radio_check import contract
from tools.nsb7_storage_check import inspect
from tools.radio_call_lifecycle_common import require_ordered
from tools.run_sip_handset_gate import verify_handset_health

TEXT_FRAME='a9e715ab7143dc08a35a6d502cbcfcc920802d544fe22c3c187e42798c1c1129'
IDLE_FRAME='32c41625e02a87a0ec97ec98a250040fc87ad8c5722c78200c54b27f7de2d560'
MENU_FRAME='76728306449ad0f2ae8cbdbd1fc5e880d3dcff054c955e8b4a2aabc87db8e7e4'


def check(text,initial,final):
    check_source(initial)
    check_authentication(text.split('gsm_call_adapter: incoming ussd id=1 result=accepted',1)[0])
    if initial!=final:
        raise ValueError('USSD notification changed persistent card contents')
    admissions=re.findall(r'incoming ussd id=1 result=accepted t=([0-9.]+)',text)
    if len(admissions)!=1 or not 24<=float(admissions[0])<25:
        raise ValueError('notification must be admitted once after settled physical unlock')
    if re.findall(r'incoming ussd state id=1 epoch=(\d+) phase=(\w+)',text)!=[
            ('1','queued'),('1','delivered')]:
        raise ValueError('notification epoch/phases differ')
    require_ordered(text,[
        ('registered PCS',r'gsm_call_adapter: network registered=1 arfcn=600\b'),
        ('admission',r'incoming ussd id=1 result=accepted'),
        ('notification',r'gsm_ss: network_initiated operation=notify transaction=0b invoke=1 dcs=0f'),
        ('REGISTER',r'GSM service downlink kind=32 sapi=0 pd=0b message=3b length=29'),
        ('return result',r'GSM service uplink sapi=0 pd=0b message=3a length=8 data=8b3a05a203020101'),
        ('release complete',r'GSM service downlink kind=27 sapi=0 pd=0b message=2a length=2'),
        ('RR release',r'LAPDm service Channel Release acknowledged'),
        ('delivered',r'incoming ussd state id=1 epoch=1 phase=delivered'),
        ('physical Read',r'nsb7_ussd_input: key=Left Softkey / Menu state=1 t=30\.000000000'),
        ('physical End',r'nsb7_ussd_input: key=End state=1 t=32\.000000000'),
        ('physical Menu',r'nsb7_ussd_input: key=Left Softkey / Menu state=1 t=34\.000000000'),
        ('complete',r'nsb7_ussd: complete'),
    ],'NSB-7 host USSD notification')
    if text.count('gsm_ss: network_initiated operation=notify ')!=1:
        raise ValueError('duplicate USSD notification')
    if 'sim_device: update ' in text or 'sim_device: unblock ' in text:
        raise ValueError('notification fixture unexpectedly writes the card')


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
        check_frame(args.run/'snap/nsb7_ussd_key_1.png',TEXT_FRAME)
        check_frame(args.run/'snap/nsb7_ussd_key_2.png',IDLE_FRAME)
        check_frame(args.run/'snap/nsb7_ussd_key_3.png',MENU_FRAME)
    except (OSError,ValueError,RuntimeError) as error:
        parser.exit(1,f'NSB-7 host USSD: FAIL: {error}\n')
    print('PASS: host USSD notification, physical Host notice reading/idle recovery, exact unchanged card; speech_tested=0')


if __name__=='__main__':main()
