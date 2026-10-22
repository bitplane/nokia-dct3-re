"""NSB-7 host alerting without connection, terminated by physical End."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))

from tools.nsb7_host_busy_check import check_physical_recovery
from tools.radio_outgoing_call_outcome_trace_check import check as check_outcome
from tools.radio_call_lifecycle_common import require_ordered
from tools.nsb7_host_busy_check import FRAMES as RECOVERY_FRAMES
from tools.nsb7_host_outgoing_call_check import check_clock
from tools.nsb7_pin_services_check import check_source
from tools.nsb7_pin_check import check_frame
from tools.nsb7_radio_check import contract
from tools.nsb7_storage_check import inspect

FRAMES={name:digest for name,digest in RECOVERY_FRAMES.items()
        if name!='nsb7_host_outgoing_failure.png'}
FRAMES['nsb7_host_outgoing_failure.png']='a70da65ada11a0f8b2fdd05d963fda9c41f85e995b7c5b74b4dcb97a2df397e4'


def check(text):
    check_physical_recovery(text)
    check_outcome(text,'no-answer',require_deferred_decision=True)
    require_ordered(text,(
        ('settled idle',r'nsb7_host_outgoing_ready: t=45\.000000000'),
        ('physical Send',r'nsb7_outgoing_call_input: key=Send state=1 t=54\.'),
        ('host request',r'gsm_call_adapter: request id=1 epoch=1 digits=5551234 clients=1'),
        ('wrong ID rejected',r'gsm_call_adapter: decision id=2 outcome=1 result=rejected'),
        ('host decision',r'gsm_call_adapter: decision id=1 outcome=2 result=accepted'),
        ('duplicate rejected',r'gsm_call_adapter: decision id=1 outcome=2 result=rejected'),
        ('host alerting',r'gsm_call_adapter: state id=1 epoch=1 phase=alerting'),
        ('physical End',r'nsb7_outgoing_call_input: key=End state=1 t=66\.'),
        ('handset Disconnect',r'GSM service uplink sapi=0 pd=03 message=25 length=5'),
        ('network Release',r'GSM service downlink kind=\d+ sapi=0 pd=03 message=2d length=2'),
        ('handset Release Complete',r'GSM service uplink sapi=0 pd=03 message=2a length=2'),
        ('host ended',r'gsm_call_adapter: state id=1 epoch=1 phase=ended'),
        ('recovery Menu',r'nsb7_outgoing_call_input: key=Left Softkey / Menu state=1 t=70\.'),
        ('recovery End',r'nsb7_outgoing_call_input: key=End state=1 t=73\.'),
        ('finished',r'nsb7_outgoing_call_done: t=76\.000000000'),
    ),'NSB-7 unanswered host call')
    if re.findall(r'gsm_call_adapter: state id=1 epoch=(\d+) phase=(\w+)',text)!=[
            ('1','alerting'),('1','ended')]:
        raise ValueError('unanswered host call phase sequence differs')
    decisions=re.findall(r'gsm_call_adapter: decision id=1 outcome=(\d+) result=accepted',text)
    if decisions!=['2'] or 'outgoing decision queued id=1 outcome=0' in text:
        raise ValueError('unanswered call must accept only its one host decision')
    if any(int(value) for value in re.findall(r'media_(?:uplink|downlink)_sequence=(\d+)',text)):
        raise ValueError('unanswered call unexpectedly exchanged host media')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image',type=Path)
    parser.add_argument('run',type=Path)
    args=parser.parse_args()
    try:
        contract(args.image.read_bytes())
        text=(args.run/'error.log').read_text()
        check(text)
        check_clock(text,control_values=('00',))
        initial=(args.run/'initial_sim_card.bin').read_bytes()
        check_source(initial)
        storage=args.run/'nvram/nsb7hle'
        if (storage/'sim_card').read_bytes()!=initial:
            raise ValueError('unanswered call changed complete SIM card')
        inspect((storage/'flash').read_bytes())
        for name,digest in FRAMES.items():
            check_frame(args.run/'snap'/name,digest)
    except (OSError,ValueError) as error:
        parser.exit(1,f'NSB-7 host no answer: FAIL: {error}\n')
    print('PASS: host-correlated alerting, no CONNECT, physical release and menu recovery')


if __name__=='__main__':
    main()
