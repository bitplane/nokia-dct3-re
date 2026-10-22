"""Exact NSB-7 open Toolkit menu restoration and physical envelope replay."""
import argparse
from pathlib import Path
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.dct3_toolkit_check import menu_events
from tools.nsb7_pin_services_check import check_authentication
from tools.nsb7_pin_check import check_frame
from tools.nsb7_radio_check import contract
from tools.nsb7_state_check import check_architecture
from tools.nsb7_storage_check import inspect
from tools.nsb7_toolkit_check import MENU_FRAMES,check_card
from tools.sim_toolkit_trace_check import require_in_order

TOKENS=('TX packet','RX enqueue','GSM service','sim_device:',
        'SIM status','SIM completion','ccont_rtc:','ccont_analog:')
FRAMES={name:digest for name,digest in MENU_FRAMES.items()
        if name not in ('nsb7_toolkit_menu_select.png','nsb7_toolkit_menu_exit.png')}
for phase in ('reference','restored'):
    FRAMES[f'nsb7_state_toolkit_menu_{phase}.png']=MENU_FRAMES['nsb7_toolkit_menu_open.png']
FRAMES['nsb7_state_toolkit_menu_final.png']=MENU_FRAMES['nsb7_toolkit_menu_exit.png']


def check(text):
    check_authentication(text.split('nsb7_state: event=saved',1)[0])
    check_architecture(text,'toolkit_menu',TOKENS)
    compact=text.replace('[:sim_card] ','')
    events=menu_events('nsb7')
    require_in_order(compact,events[:events.index('nsb7_toolkit_interactive: action=menu_select')])
    if compact.count('header cla=a0 ins=c2 ')!=2:
        raise ValueError('expected exactly two replayed menu envelopes')
    for phase in ('reference','restored'):
        begin=compact.index(f'state_replay: phase={phase} event=begin')
        end=compact.index(f'state_replay: phase={phase} event=end',begin)
        interval=compact[begin:end]
        require_in_order(interval,(
            f'nsb7_toolkit_state: action=menu_select phase={phase}',
            'nsb7_key_decode: key=19 observer_writes=0',
            'header cla=a0 ins=c2 p1=00 p2=00 p3=09',
            'envelope data=d30702020181100101',
            'menu selection item=1 accepted','SIM status ins=c2 sw=9000'))
        if interval.count('header cla=a0 ins=c2 ')!=1:
            raise ValueError('selection envelope did not occur once per replay')
    require_in_order(compact,(
        'state_replay: phase=restored event=end',
        'nsb7_toolkit_state_input: key=End state=1 t=90.000000000',
        'nsb7_key_decode: key=0f observer_writes=0',
        'nsb7_toolkit_state_input: key=End state=0 t=90.200000000'))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image',type=Path)
    parser.add_argument('run',type=Path)
    args=parser.parse_args()
    try:
        contract(args.image.read_bytes())
        check((args.run/'error.log').read_text())
        storage=args.run/'nvram/nsb7hle'
        check_card((args.run/'initial_sim_card.bin').read_bytes(),(storage/'sim_card').read_bytes())
        inspect((storage/'flash').read_bytes())
        for name,digest in FRAMES.items():
            check_frame(args.run/'snap'/name,digest)
    except (OSError,ValueError) as error:
        parser.exit(1,f'NSB-7 Toolkit state: FAIL: {error}\n')
    print('PASS: exact ARM/RAM/time, physical envelope/SIM/RTC/radio replay, exact frames and unchanged card')


if __name__=='__main__':
    main()
