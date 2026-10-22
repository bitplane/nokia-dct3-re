"""NSB-7 host busy rejection without traffic and physical UI recovery."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsb7_pin_services_check import check_authentication,check_source
from tools.nsb7_outgoing_call_check import check_inputs
from tools.radio_outgoing_host_adapter_trace_check import check as check_host
from tools.radio_outgoing_call_outcome_trace_check import check as check_outcome
from tools.nsb7_pin_check import check_frame
from tools.nsb7_radio_check import contract
from tools.nsb7_storage_check import inspect
from tools.nsb7_host_outgoing_call_check import check_clock,FRAMES as CONNECTED_FRAMES
from tools.radio_call_lifecycle_common import require_ordered

FRAMES={
    'nsb7_host_outgoing_ready.png':CONNECTED_FRAMES['nsb7_host_outgoing_ready.png'],
    'nsb7_host_outgoing_failure.png':'bacc3afa15e65b9564a873d421beb22166a3495dbdf5c97c94eb69d08a9d4303',
    'nsb7_outgoing_call_70.png':'76728306449ad0f2ae8cbdbd1fc5e880d3dcff054c955e8b4a2aabc87db8e7e4',
    'nsb7_outgoing_call_final.png':CONNECTED_FRAMES['nsb7_outgoing_call_final.png'],
}


def check_physical_recovery(text):
    check_authentication(text.split('nsb7_host_outgoing_ready:',1)[0])
    boundary='nsb7_outgoing_call_input: key=Left Softkey / Menu state=1 t=70.'
    primary=check_inputs(text.split(boundary,1)[0],delay=26)
    rows=list(re.finditer(r'nsb7_outgoing_call_input: key=(.*?) state=([01]) t=([0-9.]+)',text))
    expected=[(key,edge,f'{second}.{fraction}') for key,second in [('Left Softkey / Menu',70),('End',73)]
              for edge,fraction in [('1','000000000'),('0','200000000')]]
    if [row.groups() for row in rows[len(primary):]]!=expected:
        raise ValueError('busy recovery Menu/End sequence differs')
    for index in range(0,len(rows),2):
        key=rows[index][1]
        code=0x19 if key=='Left Softkey / Menu' else 0x0e if key=='Send' else 0x0f if key=='End' else int(key[-1])
        if f'nsb7_key_decode: key={code:02x} observer_writes=0' not in text[rows[index].end():rows[index+1].start()]:
            raise ValueError('busy/recovery physical contact did not decode')
def check(text):
    check_physical_recovery(text)
    check_host(text)
    check_outcome(text,'busy',require_deferred_decision=True)
    require_ordered(text,[
        ('settled readiness',r'nsb7_host_outgoing_ready: t=45\.000000000'),
        ('physical dial',r'nsb7_outgoing_call_input: key=Keypad 5 state=1 t=46\.'),
        ('physical Send',r'nsb7_outgoing_call_input: key=Send state=1 t=54\.'),
        ('host busy decision',r'gsm_call_adapter: decision id=1 outcome=1 result=accepted'),
        ('host ended',r'gsm_call_adapter: state id=1 epoch=1 phase=ended'),
        ('physical End',r'nsb7_outgoing_call_input: key=End state=1 t=66\.'),
        ('recovery Menu',r'nsb7_outgoing_call_input: key=Left Softkey / Menu state=1 t=70\.'),
        ('recovery End',r'nsb7_outgoing_call_input: key=End state=1 t=73\.'),
        ('complete',r'nsb7_outgoing_call_done: t=76\.000000000'),
    ],'NSB-7 busy UI recovery')
    if re.findall(r'gsm_call_adapter: state id=1 epoch=(\d+) phase=(\w+)',text)!=[('1','ended')]:
        raise ValueError('busy must end without a connected host phase')
    if 'nsb7_host_outgoing_ready: t=45.000000000' not in text or \
            'nsb7_outgoing_call_done: t=76.000000000' not in text:
        raise ValueError('busy settled-start/final observation incomplete')


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
        storage=args.run/'nvram/nsb7hle'
        initial=(args.run/'initial_sim_card.bin').read_bytes()
        check_source(initial)
        if (storage/'sim_card').read_bytes()!=initial:
            raise ValueError('busy/recovery changed persistent SIM storage')
        inspect((storage/'flash').read_bytes())
        for name,digest in FRAMES.items():
            check_frame(args.run/'snap'/name,digest)
    except (OSError,ValueError) as error:
        parser.exit(1,f'NSB-7 host busy: FAIL: {error}\n')
    print('PASS: correlated busy clears without traffic/CONNECT, exact ended-call UI and physical Menu/End recovery; unchanged card')


if __name__=='__main__':
    main()
