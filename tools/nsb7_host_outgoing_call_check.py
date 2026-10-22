"""Authenticated physical NSB-7 host-decided outgoing signaling, no media claim."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsb7_pin_services_check import check_authentication,check_source
from tools.nsb7_outgoing_call_check import check as check_call,check_inputs
from tools.nsb7_pin_check import check_frame
from tools.nsb7_radio_check import contract
from tools.nsb7_storage_check import inspect
from tools.nsb7_netmonitor_check import IDLE_HASH
from tools.radio_call_lifecycle_common import require_ordered

FRAMES={
    'nsb7_host_outgoing_ready.png':IDLE_HASH,
    'nsb7_outgoing_call_52.png':'223ffbdc6b19f01641f293b0a168a65b8715c37b71a97ecc5bb972b8d15baea1',
    'nsb7_outgoing_call_54.png':'067a6a8ee85b4989a4295780ab87d1fe2eccbab47271107e8550dacc02883091',
    'nsb7_host_outgoing_connected.png':'a8e9a74c4cccefe0bfa934225e5551e574e43da80e02f36d447b7622653c2e29',
    'nsb7_outgoing_call_final.png':IDLE_HASH,
}


def check(text, restored=False):
    check_authentication(text.split('nsb7_host_outgoing_ready:',1)[0])
    check_call(text,delay=26)
    rows=check_inputs(text,delay=26)
    for index in range(0,len(rows),2):
        key=rows[index][1]
        code=0x0e if key=='Send' else 0x0f if key=='End' else int(key[-1])
        if f'nsb7_key_decode: key={code:02x} observer_writes=0' not in text[rows[index].end():rows[index+1].start()]:
            raise ValueError('host outgoing physical contact did not decode')
    require_ordered(text,[
        ('settled readiness',r'nsb7_host_outgoing_ready: t=45\.000000000'),
        ('physical dial',r'nsb7_outgoing_call_input: key=Keypad 5 state=1 t=46\.'),
        ('physical Send',r'nsb7_outgoing_call_input: key=Send state=1 t=54\.'),
        ('host publication',r'gsm_call_adapter: request id=1 epoch=1 digits=5551234 clients=1'),
        ('wrong ID',r'gsm_call_adapter: decision id=2 outcome=1 result=rejected'),
        ('queued decision',r'outgoing decision queued id=1 outcome=0'),
        ('consumed decision',r'outgoing decision consumed id=1 outcome=0'),
        ('accepted decision',r'gsm_call_adapter: decision id=1 outcome=0 result=accepted'),
        ('duplicate decision',r'gsm_call_adapter: decision id=1 outcome=0 result=rejected'),
        ('host connected',r'gsm_call_adapter: state id=1 epoch=1 phase=connected'),
        ('physical End',r'nsb7_outgoing_call_input: key=End state=1 t=66\.'),
        ('host closed',rf'gsm_call_adapter: state id=1 epoch={2 if restored else 1} phase=media_closed'),
        ('host ended',rf'gsm_call_adapter: state id=1 epoch={2 if restored else 1} phase=ended'),
    ],'NSB-7 host outgoing signaling')
    for marker in ('outgoing decision queued id=1 outcome=0',
                   'outgoing decision consumed id=1 outcome=0',
                   'gsm_call_adapter: decision id=1 outcome=0 result=accepted'):
        if text.count(marker)!=1:
            raise ValueError('host outgoing decision was consumed more than once')
    expected=[('1','connected')]
    if restored:
        expected.append(('2','connected'))
        require_ordered(text,[
            ('connected predecessor',r'gsm_call_adapter: state id=1 epoch=1 phase=connected'),
            ('save',r'nsb7_state: event=saved'),('restore',r'nsb7_state: event=restored'),
            ('new epoch',r'gsm_call_adapter: state id=1 epoch=2 phase=connected'),
            ('stale decision rejection',r'gsm_call_adapter: stale epoch=1 current=2 type=decision id=1 result=rejected'),
            ('physical End',r'nsb7_outgoing_call_input: key=End state=1 t=66\.'),
        ],'NSB-7 host call restoration')
    expected += [(str(2 if restored else 1),'media_closed'),(str(2 if restored else 1),'ended')]
    if re.findall(r'gsm_call_adapter: state id=1 epoch=(\d+) phase=(\w+)',text)!=expected:
        raise ValueError('host outgoing identity/epoch lifecycle differs')


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
            raise ValueError('host outgoing call changed persistent SIM storage')
        inspect((storage/'flash').read_bytes())
        for name,digest in FRAMES.items():
            check_frame(args.run/'snap'/name,digest)
    except (OSError,ValueError) as error:
        parser.exit(1,f'NSB-7 host outgoing: FAIL: {error}\n')
    print('PASS: settled authenticated physical dial, correlated host connect decision and physical release; exact card/UI, speech unproved')


if __name__=='__main__':
    main()
