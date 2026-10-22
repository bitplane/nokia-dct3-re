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
from tools.radio_host_ussd_trace_check import verify as verify_host_request
from tools.nsb7_state_check import check_architecture_snapshot
from tools.nsb7_host_outgoing_call_check import check_clock

TEXT_FRAME='a9e715ab7143dc08a35a6d502cbcfcc920802d544fe22c3c187e42798c1c1129'
IDLE_FRAME='32c41625e02a87a0ec97ec98a250040fc87ad8c5722c78200c54b27f7de2d560'
MENU_FRAME='76728306449ad0f2ae8cbdbd1fc5e880d3dcff054c955e8b4a2aabc87db8e7e4'
RESPONSE_FRAME='8d5e62e06c9a690aac24175be97578644540c612ae9764899df3b88cadf74bef'
PENDING_FRAME='58b9e1bc48951b8aa0bad27d4414d1c15a4b9984e265328b92c7342b27b14eb3'


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


def check_request(text,initial,final,restored=False):
    check_source(initial)
    check_authentication(text.split('nsb7_ussd_input:',1)[0])
    if initial!=final:
        raise ValueError('USSD request changed persistent card contents')
    verify_host_request(text,require_restore=restored)
    epoch='2' if restored else '1'
    if restored:
        check_architecture_snapshot(text,'ussd')
        if re.findall(r'gsm_call_adapter: ussd request id=1 epoch=(\d+)',text)!=['1','2']:
            raise ValueError('pending USSD must republish once under the restored epoch')
        before=text.split('nsb7_state: event=restored',1)[0]
        if 'ussd response id=' in before or 'GSM service downlink kind=27 sapi=0 pd=0b' in before:
            raise ValueError('USSD received a host result before restoration')
        require_ordered(text,[
            ('pending request',r'gsm_call_adapter: ussd request id=1 epoch=1'),
            ('save',r'nsb7_state: event=saved'),
            ('restore',r'nsb7_state: event=restored'),
            ('republication',r'gsm_call_adapter: ussd request id=1 epoch=2'),
            ('stale epoch rejected',r'ussd response id=1 outcome=0 result=rejected'),
            ('wrong request rejected',r'ussd response id=2 outcome=0 result=rejected'),
            ('matching response',r'ussd response id=1 outcome=0 result=accepted'),
        ],'pending USSD host restoration')
        # Only the restored branch contributes to final storage/time. The
        # reference intentionally has no host decision, so traffic is unequal.
        continuous=text.split('nsb7_state: event=saved',1)[0]+text.split('nsb7_state: event=restored',1)[1]
        check_clock(continuous,finish=43,control_values=('00',))
    expected=[(key,str(second)) for second,key in (
        (26,'Keypad *'),(27,'Keypad 1'),(28,'Keypad 2'),(29,'Keypad 3'),
        (30,'Keypad #'),(32,'Send'),(38,'End'),(40,'Left Softkey / Menu'))]
    presses=re.findall(r'nsb7_ussd_input: key=(.*?) state=1 t=(\d+)\.000000000',text)
    if presses!=expected:
        raise ValueError('physical USSD dial/recovery sequence differs')
    releases=re.findall(r'nsb7_ussd_input: key=(.*?) state=0 t=(\d+)\.200000000',text)
    if releases!=expected:
        raise ValueError('USSD physical contacts were not released')
    if re.findall(r'ussd state id=1 epoch=(\d+) phase=(\w+)',text)!=[
            (epoch,'accepted'),(epoch,'ended')]:
        raise ValueError('host USSD request state lifecycle differs')
    if text.count('gsm_ss: request=ussd_host ')!=1:
        raise ValueError('expected one firmware USSD request')
    require_ordered(text,[
        ('registered PCS',r'gsm_call_adapter: network registered=1 arfcn=600\b'),
        ('physical Send',r'nsb7_ussd_input: key=Send state=1 t=32\.'),
        ('own REGISTER',r'GSM service uplink sapi=0 pd=0b message=3b length=27 data=1b7b1c14a11202010102013b300a04010f0405aa986c36027f0100'),
        ('correlated response',r'ussd response id=1 outcome=0 result=accepted'),
        ('release complete',r'GSM service downlink kind=27 sapi=0 pd=0b message=2a length=32'),
        ('RR release',r'LAPDm service Channel Release acknowledged'),
        ('ended',rf'ussd state id=1 epoch={epoch} phase=ended'),
        ('physical End',r'nsb7_ussd_input: key=End state=1 t=38\.'),
        ('physical Menu',r'nsb7_ussd_input: key=Left Softkey / Menu state=1 t=40\.'),
        ('complete',r'nsb7_ussd: complete'),
    ],'NSB-7 host USSD request')
    if 'sim_device: update ' in text:
        raise ValueError('USSD request unexpectedly writes SIM records')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image',type=Path)
    parser.add_argument('run',type=Path)
    parser.add_argument('--console',type=Path,required=True)
    parser.add_argument('--request',action='store_true',help='physical *123# with host response')
    parser.add_argument('--restore',action='store_true',help='pending mobile-originated request restoration')
    args=parser.parse_args()
    if args.restore and not args.request:parser.error('--restore requires --request')
    try:
        contract(args.image.read_bytes())
        verify_handset_health(args.console.read_text())
        storage=args.run/'nvram/nsb7hle'
        inspect((storage/'flash').read_bytes())
        inputs=((args.run/'error.log').read_text(),(args.run/'initial_sim_card.bin').read_bytes(),
                (storage/'sim_card').read_bytes())
        if args.request:check_request(*inputs,restored=args.restore)
        else:check(*inputs)
        if args.restore and not (args.run/'sta/nsb7hle/nsb7_ussd.sta').is_file():
            raise ValueError('actual pending-USSD snapshot absent')
        if args.restore:
            check_frame(args.run/'snap/nsb7_ussd_pending_reference.png',PENDING_FRAME)
        if args.request:
            for second in (36,37):
                check_frame(args.run/f'snap/nsb7_ussd_response_{second}.png',RESPONSE_FRAME)
            check_frame(args.run/'snap/nsb7_ussd_key_7.png',IDLE_FRAME)
            check_frame(args.run/'snap/nsb7_ussd_key_8.png',MENU_FRAME)
        else:
            check_frame(args.run/'snap/nsb7_ussd_key_1.png',TEXT_FRAME)
            check_frame(args.run/'snap/nsb7_ussd_key_2.png',IDLE_FRAME)
            check_frame(args.run/'snap/nsb7_ussd_key_3.png',MENU_FRAME)
    except (OSError,ValueError,RuntimeError) as error:
        parser.exit(1,f'NSB-7 host USSD: FAIL: {error}\n')
    print('PASS: host USSD '+('physical *123# request/Host network response' if args.request else 'notification/Host notice reading')+
          ', exact idle/menu recovery and unchanged card; speech_tested=0')


if __name__=='__main__':main()
