"""Authenticated NSB-7 DISPLAY TEXT and physical successful dismissal."""
import argparse
from pathlib import Path
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.dct3_toolkit_check import verify_display_text
from tools.nsb7_pin_services_check import check_authentication,check_source
from tools.nsb7_pin_check import check_frame
from tools.nsb7_radio_check import contract
from tools.nsb7_storage_check import inspect
from tools.radio_call_lifecycle_common import require_ordered

FRAMES={
    'nsb7_toolkit_display.png':'0b61ca941a012e9feffe2d13b834c720031b71295220fc32139decd034172b2c',
    'nsb7_toolkit_after_dismiss.png':'32c41625e02a87a0ec97ec98a250040fc87ad8c5722c78200c54b27f7de2d560',
}


def check(text):
    check_authentication(text.split('nsb7_toolkit_physical:',1)[0])
    verify_display_text(text,'nsb7')
    for instruction in ('10','12','14'):
        if text.count(f'header cla=a0 ins={instruction} ')!=1:
            raise ValueError('expected one profile/FETCH/terminal-response exchange')
    require_ordered(text,[
        ('physical OK',r'nsb7_toolkit_physical: action=dismiss'),
        ('decoded softkey',r'nsb7_key_decode: key=19 observer_writes=0'),
        ('terminal response',r'header cla=a0 ins=14 p1=00 p2=00 p3=0c'),
    ],'NSB-7 physical Toolkit dismissal')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image',type=Path)
    parser.add_argument('run',type=Path)
    args=parser.parse_args()
    try:
        contract(args.image.read_bytes())
        check((args.run/'error.log').read_text())
        initial=(args.run/'initial_sim_card.bin').read_bytes()
        check_source(initial)
        storage=args.run/'nvram/nsb7hle'
        if (storage/'sim_card').read_bytes()!=initial:
            raise ValueError('Toolkit changed persistent SIM storage')
        inspect((storage/'flash').read_bytes())
        for name,digest in FRAMES.items():
            check_frame(args.run/'snap'/name,digest)
    except (OSError,ValueError) as error:
        parser.exit(1,f'NSB-7 Toolkit: FAIL: {error}\n')
    print('PASS: authenticated registered DISPLAY TEXT, physical OK/result00, exact card/UI')


if __name__=='__main__':
    main()
