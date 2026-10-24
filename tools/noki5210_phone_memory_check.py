"""Own NSM-5 phone-memory save/cold read, kept separate from SIM phonebook proof."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.make_sim_card_profile import make_profile
from tools.noki5210_pin_registration_check import verify_coherent_late, FRAMES
from tools.radio_call_lifecycle_common import require_ordered


SAVE_KEYS = (['Right Softkey / C'] + ['Scroll Down'] * 4 +
             ['Left Softkey / Menu'] * 2 + ['Scroll Down', 'Left Softkey / Menu', 'End',
             'Right Softkey / C', 'Left Softkey / Menu', 'Left Softkey / Menu', 'End',
             'Right Softkey / C', 'Scroll Down', 'Left Softkey / Menu',
             'Keypad 2', 'Keypad 3', 'Keypad 2', 'Left Softkey / Menu',
             'Keypad 1', 'Keypad 2', 'Keypad 3', 'Left Softkey / Menu',
             'End', 'Left Softkey / Menu', 'End'])
READ_KEYS = ['Right Softkey / C'] + ['Left Softkey / Menu'] * 3 + ['End', 'Left Softkey / Menu', 'End']
SAVE_FRAMES = {
    'phone_empty': '5b617392ff03236596d815bc50b1dfd891228d13ca6557728b18496f8db59bb3',
    'memory_selected': 'e8a0189272eb64a815ba67304938418a36abdd57223bba000b0c1a8f258ed98a',
    'phone_name': '66d49547236c2770850aa90093abdf799bb0f13b3a9fa3bdd467159aee0be021',
    'phone_number': '65f009af08ab47a4b23fc72d3980a48da140d849738e20e0ba29d78ea9c1847c',
    'phone_saved': '194a944dc18e6c7699b4f9a7bcaa32d1fcdfd48b7dd614bc365c65d7af4419aa',
}
READ_FRAMES = {
    'phonebook_list': '8399986258ce449c38d8235e7a130c3c7c25d7b961398feaffb86769c63898cf',
    'phonebook': 'fd76ae62168f74fec8f6931d53888a2ea9c8c52f754cc19d3c0a8bc914ae18e9',
}


def expected_card():
    card = bytearray(make_profile(True) + bytes.fromhex('00f110') + b'\xff' * 43)
    card[1604:1609] = bytes.fromhex('00f1100001')
    card[1610] = 0
    return bytes(card)


def verify(text, card, *, read=False):
    verify_coherent_late(text, card, preserved=read)
    if card != expected_card() or re.search(r'header cla=a0 ins=dc\b|update fid=6f3a\b', text):
        raise ValueError('phone-memory fixture changed SIM storage')
    mode = 'read' if read else 'save'
    for edge in ('begin', 'complete'):
        if text.count(f'5210_phone_memory: mode={mode} {edge}=1') != 1:
            raise ValueError('phone-memory fixture did not execute exactly once')
    keys = re.findall(r'5210_phone_memory_key: name=(.*?) released=1', text)
    if keys != (READ_KEYS if read else SAVE_KEYS) or text.count('5210_phone_memory_key:') != len(keys):
        raise ValueError('phone-memory physical input/release sequence differs')
    require_ordered(text, (
        ('PIN', r'SIM status ins=20 sw=9000'),
        ('registered', r'LAPDm Location Updating Accept acknowledged'),
        ('begin', rf'5210_phone_memory: mode={mode} begin=1'),
        ('complete', rf'5210_phone_memory: mode={mode} complete=1'),
    ), '5210 own phone-memory storage')


def check_frames(directory, *, read=False):
    from PIL import Image
    frames = dict(READ_FRAMES if read else SAVE_FRAMES)
    frames.update({key: FRAMES[key] for key in ('entered', 'idle', 'recovered')})
    for phase, expected in frames.items():
        with Image.open(directory / f'5210_pin_{phase}.png') as image:
            if image.size != (84, 48) or hashlib.sha256(image.convert('L').tobytes()).hexdigest() != expected:
                raise ValueError('phone-memory reviewed frame differs: ' + phase)
    # Exclude the animated envelope and scrollbar; retain the reviewed title.
    with Image.open(directory / '5210_pin_menu.png') as image:
        if (image.size != (84, 48) or hashlib.sha256(image.convert('L').crop((0, 0, 72, 16)).tobytes()).hexdigest()
                != '1e5c11fcea9aac5331e18c0070697e8795003d6de9a0250284b3d9605209e654'):
            raise ValueError('phone-memory Messages recovery differs')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    modes = parser.add_mutually_exclusive_group(required=True)
    modes.add_argument('--save', action='store_true')
    modes.add_argument('--read', action='store_true')
    args = parser.parse_args()
    try:
        verify((args.run / 'error.log').read_text(),
               (args.run / 'nvram/noki5210_3/sim_card').read_bytes(), read=args.read)
        check_frames(args.run / 'snap', read=args.read)
        if (args.run / 'nvram/noki5210_3/flash').stat().st_size != 0x400000:
            raise ValueError('missing complete own-product flash NVRAM')
    except (OSError, ValueError) as error:
        parser.exit(1, f'5210 phone-memory FAIL: {error}\n')
    print('PASS: own 5210 phone-memory contact, physical recovery and complete unchanged SIM')


if __name__ == '__main__':
    main()
