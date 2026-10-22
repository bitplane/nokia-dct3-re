"""Authenticated NSB-7 Toolkit display/input and physical successful responses."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.dct3_toolkit_check import verify_display_text,verify_interactive,verify_menu
from tools.nsb7_pin_services_check import check_authentication,check_source
from tools.nsb7_pin_check import check_frame
from tools.nsb7_radio_check import contract
from tools.nsb7_storage_check import inspect
from tools.radio_call_lifecycle_common import require_ordered

FRAMES={
    'nsb7_toolkit_display.png':'0b61ca941a012e9feffe2d13b834c720031b71295220fc32139decd034172b2c',
    'nsb7_toolkit_after_dismiss.png':'32c41625e02a87a0ec97ec98a250040fc87ad8c5722c78200c54b27f7de2d560',
}
INTERACTIVE_FRAMES={
    'nsb7_toolkit_display.png':FRAMES['nsb7_toolkit_display.png'],
    'nsb7_toolkit_inkey.png':'e94ea9d879033d48822bb76a8f080f95e12456c3b897a1717be10fb4ddbbb77f',
    'nsb7_toolkit_input.png':'2c7ca84fb1f98d4a782fa3b3cc5f8f5fc0eadf68b409bbee9ebd3bdc70214597',
    'nsb7_toolkit_input_filled.png':'3b003249f2e069abd5a91b9acd05b2ffe1b0732a69d6589e60d18c4e70f71df1',
    'nsb7_toolkit_interactive_final.png':'29dcc2200f6f5c170afea80e751d1d3afefd83332964c4bdc0f2a843be5e2567',
}
MENU_FRAMES={
    **INTERACTIVE_FRAMES,
    'nsb7_toolkit_menu.png':'76728306449ad0f2ae8cbdbd1fc5e880d3dcff054c955e8b4a2aabc87db8e7e4',
    'nsb7_toolkit_menu_skip_netmonitor.png':'8bc22a997848abf796726c946c9936d051263ff4f8301f6fbd59275e074b848a',
    'nsb7_toolkit_menu_last.png':'d25fbda213ecf2bf85918ea4daf584036e45ef112c6172a7b48a4d6356acb494',
    'nsb7_toolkit_menu_open.png':'a5a9b402a8ca6111d5b4fd24ab253a6bc98f73f8119d11bc3bca646915e66b8f',
    'nsb7_toolkit_menu_select.png':'a5a9b402a8ca6111d5b4fd24ab253a6bc98f73f8119d11bc3bca646915e66b8f',
    'nsb7_toolkit_menu_exit.png':INTERACTIVE_FRAMES['nsb7_toolkit_interactive_final.png'],
}


def check(text, interactive=False, menu=False):
    interactive=interactive or menu
    marker='nsb7_toolkit_interactive' if interactive else 'nsb7_toolkit_physical'
    check_authentication(text.split(marker+':',1)[0])
    verifier=verify_menu if menu else verify_interactive if interactive else verify_display_text
    verifier(text,'nsb7')
    for instruction in ('10','12','14'):
        expected=(4 if menu else 3) if interactive and instruction!='10' else 1
        if text.count(f'header cla=a0 ins={instruction} ')!=expected:
            raise ValueError('Toolkit profile/FETCH/terminal-response count differs')
    if text.count('header cla=a0 ins=c2 ')!=(1 if menu else 0):
        raise ValueError('Toolkit envelope count differs')
    if interactive:
        actions=list(re.finditer(r'nsb7_toolkit_interactive: action=(\w+)',text))
        expected=[('dismiss','19'),('inkey_5','05'),('inkey_confirm','19'),
                  ('input_4','04'),('input_2','02'),('confirm','19')]
        if menu:
            expected += [('menu','19'),('menu_skip_netmonitor','17'),('menu_last','17'),
                         ('menu_open','19'),('menu_select','19'),('menu_exit','0f')]
        if [action[1] for action in actions]!=[name for name,_ in expected]:
            raise ValueError('physical Toolkit input sequence differs')
        for index,(_,code) in enumerate(expected):
            end=actions[index+1].start() if index+1<len(actions) else len(text)
            for instruction in ('14','c2'):
                response=text.find(f'header cla=a0 ins={instruction} ',actions[index].end())
                if response>=0:
                    end=min(end,response)
            if f'nsb7_key_decode: key={code} observer_writes=0' not in text[actions[index].end():end]:
                raise ValueError('physical Toolkit contact did not decode')
        return
    require_ordered(text,[
        ('physical OK',r'nsb7_toolkit_physical: action=dismiss'),
        ('decoded softkey',r'nsb7_key_decode: key=19 observer_writes=0'),
        ('terminal response',r'header cla=a0 ins=14 p1=00 p2=00 p3=0c'),
    ],'NSB-7 physical Toolkit dismissal')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image',type=Path)
    parser.add_argument('run',type=Path)
    parser.add_argument('--interactive',action='store_true')
    parser.add_argument('--menu',action='store_true')
    args=parser.parse_args()
    try:
        contract(args.image.read_bytes())
        check((args.run/'error.log').read_text(),interactive=args.interactive,menu=args.menu)
        initial=(args.run/'initial_sim_card.bin').read_bytes()
        check_source(initial)
        storage=args.run/'nvram/nsb7hle'
        if (storage/'sim_card').read_bytes()!=initial:
            raise ValueError('Toolkit changed persistent SIM storage')
        inspect((storage/'flash').read_bytes())
        frames=MENU_FRAMES if args.menu else INTERACTIVE_FRAMES if args.interactive else FRAMES
        for name,digest in frames.items():
            check_frame(args.run/'snap'/name,digest)
    except (OSError,ValueError) as error:
        parser.exit(1,f'NSB-7 Toolkit: FAIL: {error}\n')
    print('PASS: authenticated registered Toolkit '+
          ('menu selection' if args.menu else 'DISPLAY TEXT/GET INKEY/GET INPUT' if args.interactive else 'DISPLAY TEXT')+
          ', physical successful responses, exact card/UI')


if __name__=='__main__':
    main()
