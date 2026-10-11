"""Verify physical NSM-3 Toolkit menu selection across emulator restoration."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.noki8210_toolkit_inkey_check import verify_protocol, MENU_FRAMES
from tools.noki8210_registration_check import verify as registration
from tools.noki8210_staged_check import verify as stage
from tools.radio_state_roundtrip import verify_roundtrip
from tools.sim_toolkit_trace_check import require_in_order


def verify(text):
    if '8210_menu_state: FAIL' in text:
        raise ValueError('menu restoration fixture incomplete')
    verify_protocol(text, menu=True)
    states = re.findall(r'8210_menu_state: event=(saved|restored) pc=(\w+) sp=(\w+) ram=(\w+) t=([0-9.]+)', text)
    if len(states) != 2 or [s[0] for s in states] != ['saved', 'restored'] or states[0][1:] != states[1][1:]:
        raise ValueError('menu architectural state did not restore exactly')
    verify_roundtrip(text, ('sim_device:',), '8210 Toolkit menu')
    compact = text.replace('[:sim_card] ', '')
    for phase in ('reference', 'restored'):
        begin = compact.index(f'state_replay: phase={phase} event=begin')
        end = compact.index(f'state_replay: phase={phase} event=end', begin)
        require_in_order(compact[begin:end], (
            f'8210_toolkit_physical: action=menu_select phase={phase}',
            'header cla=a0 ins=c2 p1=00 p2=00 p3=09',
            'envelope data=d30702020181100101',
            'menu selection item=1 accepted', 'SIM status ins=c2 sw=9000',
        ))


def main():
    from PIL import Image
    import hashlib
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    run = args.log.parent
    try:
        text = args.log.read_text(errors='replace')
        verify(text)
        stage(text, runtime=True, selftest=True, base_record=True)
        registration(text, (run / 'nvram/nsm3hle/sim_card').read_bytes())
        for phase in ('entry', 'items', 'reference', 'restored'):
            with Image.open(run / f'snap/8210_toolkit_menu_{phase}.png') as frame:
                expected = MENU_FRAMES['menu_entry' if phase == 'entry' else 'menu_items']
                if frame.size != (84, 48) or hashlib.sha256(frame.convert('L').tobytes()).hexdigest() != expected:
                    raise ValueError('unreviewed menu restoration pixels: ' + phase)
    except (OSError, ValueError) as error:
        parser.exit(1, f'8210 Toolkit restoration FAIL: {error}\n')
    print('8210 physical Toolkit selection, exact state/card replay and pixels PASS; native DSP unproved')


if __name__ == '__main__':
    main()
