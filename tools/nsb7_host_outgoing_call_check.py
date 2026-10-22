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
from tools.radio_call_lifecycle_common import require_ordered

FRAMES={
    # Retained 13:47:45 source; address-7 analog writes do not reset seconds.
    'nsb7_host_outgoing_ready.png':'32c41625e02a87a0ec97ec98a250040fc87ad8c5722c78200c54b27f7de2d560',
    'nsb7_outgoing_call_52.png':'17d4e52d7293047858f0c0156b24ae42a406358686e02ad45ad46c3460c78bd0',
    'nsb7_outgoing_call_54.png':'c351864a51840950319536e3d491c284c5722ba66ee25112b7dad855a016634e',
    'nsb7_host_outgoing_connected.png':'9cabd1e512ce5e4467808054cbabf5c5f1306e7c6ab681e3fa193661cc441c7e',
    'nsb7_outgoing_call_final.png':'29dcc2200f6f5c170afea80e751d1d3afefd83332964c4bdc0f2a843be5e2567',
}


def check_clock(text, finish=76, control_values=('00','0f')):
    rows=re.findall(r'ccont_rtc: event=second time=(\d+):(\d+):(\d+) day=(\d+).*? t=([\d.]+)',text)
    if len(rows)!=finish:
        raise ValueError(f'expected all {finish} retained-clock ticks')
    for index,(hour,minute,second,day,time) in enumerate(rows,1):
        absolute=int(day)*86400+int(hour)*3600+int(minute)*60+int(second)
        if float(time)!=index or absolute!=13*3600+47*60+45+index:
            raise ValueError('retained clock reset or skipped during call')
    for value in control_values:
        if not re.search(rf'ccont_analog: event=control_write reg=07 data={value}\b',text):
            raise ValueError('missing independent analog-control write')


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
        ('accepted decision',r'gsm_call_adapter: decision id=1 outcome=0 result=accepted'),
        ('duplicate decision',r'gsm_call_adapter: decision id=1 outcome=0 result=rejected'),
        ('host connected',r'gsm_call_adapter: state id=1 epoch=1 phase=connected'),
        ('physical End',r'nsb7_outgoing_call_input: key=End state=1 t=66\.'),
        ('host closed',rf'gsm_call_adapter: state id=1 epoch={2 if restored else 1} phase=media_closed'),
        ('host ended',rf'gsm_call_adapter: state id=1 epoch={2 if restored else 1} phase=ended'),
    ],'NSB-7 host outgoing signaling')
    # Adapter acknowledgment and emulation-thread consumption can interleave.
    # Both follow queueing; consumption must precede the connected handset.
    require_ordered(text,[
        ('queued decision',r'outgoing decision queued id=1 outcome=0'),
        ('consumed decision',r'outgoing decision consumed id=1 outcome=0'),
        ('host connected',r'gsm_call_adapter: state id=1 epoch=1 phase=connected'),
    ],'NSB-7 host outgoing decision consumption')
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
        text=(args.run/'error.log').read_text()
        check(text)
        check_clock(text)
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
