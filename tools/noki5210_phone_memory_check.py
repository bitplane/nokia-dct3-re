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
EDIT_KEYS = READ_KEYS[:-3] + ['Left Softkey / Menu'] * 3 + [
    'Right Softkey / C', 'Keypad 4', 'Left Softkey / Menu'] + READ_KEYS[-3:]
KEYS = {'save': SAVE_KEYS, 'read': READ_KEYS, 'edit': EDIT_KEYS, 'edit_read': READ_KEYS}
KEYS['rename'] = READ_KEYS[:-3] + ['Left Softkey / Menu'] * 2 + ['Right Softkey / C'] * 3 + [
    'Keypad 2'] + ['Left Softkey / Menu'] * 3 + READ_KEYS[-3:]
KEYS['rename_read'] = READ_KEYS
KEYS['add'] = KEYS['rename'][:-4] + ['Scroll Down'] + KEYS['rename'][-4:]
KEYS['multi_read'] = READ_KEYS[:-3] + ['End', 'Right Softkey / C',
    'Left Softkey / Menu', 'Left Softkey / Menu', 'Scroll Down',
    'Left Softkey / Menu'] + READ_KEYS[-3:]
ERASE_PREFIX = READ_KEYS[:-3] + ['Left Softkey / Menu', 'Scroll Down', 'Scroll Down', 'Left Softkey / Menu']
KEYS.update(erase_cancel=ERASE_PREFIX + ['Right Softkey / C'] + READ_KEYS[-3:],
            erase=ERASE_PREFIX + ['Left Softkey / Menu'] + READ_KEYS[-3:],
            empty_read=['Right Softkey / C'] + ['Scroll Down'] * 4 +
                       ['Left Softkey / Menu', 'End'] + READ_KEYS[:3] + READ_KEYS[-3:])
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
EDIT_FRAMES = dict(READ_FRAMES,
    edit_name=SAVE_FRAMES['phone_name'],
    edit_number='f774f5d64d22a49e8bfd0f31af25e687e4d89c944096dddaaaf7b1c3f61a07d8',
    edit_changed='10e0a6aa6666298d8f28f8214307ad24d0c3df71022b54142790963ddd175be3',
    edit_saved=SAVE_FRAMES['phone_saved'])
# The edited reader expects 124; the original read gate retains its 123 oracle.
EDIT_READ_FRAMES = dict(READ_FRAMES, phonebook='5066beda30d2fbd845ba3036385adc8db278b7844707b18b4d921ee14387775d')
MODE_FRAMES = {'save': SAVE_FRAMES, 'read': READ_FRAMES,
               'edit': EDIT_FRAMES, 'edit_read': EDIT_READ_FRAMES}
MODE_FRAMES['rename'] = dict(READ_FRAMES, edit_name=SAVE_FRAMES['phone_name'],
    rename_changed='892de07c3d691a90c01a1c35132df8b1e08ebfb9b2c6bcf7a342116dfa8bc2f7',
    rename_number=EDIT_FRAMES['edit_number'],
    rename_replace='22891ade22b4f12944faabdedbb27a7e85bc03015784aa0348b2e5f9db8f52a1',
    rename_saved=SAVE_FRAMES['phone_saved'])
# Number Details does not show the name: the cold list is the rename oracle.
MODE_FRAMES['rename_read'] = dict(READ_FRAMES,
    phonebook_list='9f88b7547ec05ef3fbdf69ad4f3bc94e0acbc1c2834595ea2ff4d85e7eb463de')
MODE_FRAMES['add'] = MODE_FRAMES['rename']
# The two independently selected names reuse the already reviewed A and Ada
# list frames. Number Details alone cannot distinguish these equal numbers.
MODE_FRAMES['multi_read'] = dict(MODE_FRAMES['rename_read'],
    second_list=READ_FRAMES['phonebook_list'], second_number=READ_FRAMES['phonebook'])
ERASE_PROMPT = 'f4798fa7bdb4077bae837b58cec453ce6153f7f463a6c2d3d51827a6841a7eaf'
MODE_FRAMES.update(
    erase_cancel=dict(READ_FRAMES, erase_prompt=ERASE_PROMPT, erase_result=READ_FRAMES['phonebook_list']),
    erase=dict(READ_FRAMES, erase_prompt=ERASE_PROMPT,
               erase_result='9b824b193e9310a0e0644d539bb352dabeea75f2f628a27c94b67a11ce8dfcf8'),
    empty_read={key: SAVE_FRAMES[key] for key in ('memory_selected', 'phone_empty')})
KEYS['erase_first'] = KEYS['erase']
MODE_FRAMES['erase_first'] = dict(MODE_FRAMES['erase'],
    phonebook_list=MODE_FRAMES['rename_read']['phonebook_list'],
    erase_prompt='3d2ffab2e4af9a377c9962ebc75e29c74439700bdef4a0e87cc9a74a366c2210',
    erase_result='56cf729fac7146f33c16fe06f42d863b2421aa6e2f0eb695287e8afa5e0300bc')


def expected_card():
    card = bytearray(make_profile(True) + bytes.fromhex('00f110') + b'\xff' * 43)
    card[1604:1609] = bytes.fromhex('00f1100001')
    card[1610] = 0
    return bytes(card)


def verify(text, card, *, mode='save'):
    if mode not in KEYS:
        raise ValueError('unreviewed phone-memory mode')
    verify_coherent_late(text, card, preserved=mode != 'save')
    if card != expected_card() or re.search(r'header cla=a0 ins=dc\b|update fid=6f3a\b', text):
        raise ValueError('phone-memory fixture changed SIM storage')
    for edge in ('begin', 'complete'):
        if text.count(f'5210_phone_memory: mode={mode} {edge}=1') != 1:
            raise ValueError('phone-memory fixture did not execute exactly once')
    keys = re.findall(r'5210_phone_memory_key: name=(.*?) released=1', text)
    if keys != KEYS[mode] or text.count('5210_phone_memory_key:') != len(keys):
        raise ValueError('phone-memory physical input/release sequence differs')
    require_ordered(text, (
        ('PIN', r'SIM status ins=20 sw=9000'),
        ('registered', r'LAPDm Location Updating Accept acknowledged'),
        ('begin', rf'5210_phone_memory: mode={mode} begin=1'),
        ('complete', rf'5210_phone_memory: mode={mode} complete=1'),
    ), '5210 own phone-memory storage')


def check_frames(directory, *, mode='save'):
    from PIL import Image
    frames = dict(MODE_FRAMES[mode])
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
    for flag, mode in (('--save', 'save'), ('--read', 'read'),
                       ('--edit', 'edit'), ('--edit-read', 'edit_read'),
                       ('--rename', 'rename'), ('--rename-read', 'rename_read'),
                       ('--add', 'add'), ('--multi-read', 'multi_read'),
                       ('--erase-cancel', 'erase_cancel'), ('--erase', 'erase'),
                       ('--erase-first', 'erase_first'),
                       ('--empty-read', 'empty_read')):
        modes.add_argument(flag, action='store_const', dest='mode', const=mode)
    args = parser.parse_args()
    try:
        verify((args.run / 'error.log').read_text(),
               (args.run / 'nvram/noki5210_3/sim_card').read_bytes(), mode=args.mode)
        check_frames(args.run / 'snap', mode=args.mode)
        if (args.run / 'nvram/noki5210_3/flash').stat().st_size != 0x400000:
            raise ValueError('missing complete own-product flash NVRAM')
    except (OSError, ValueError) as error:
        parser.exit(1, f'5210 phone-memory FAIL: {error}\n')
    print('PASS: own 5210 phone-memory contact, physical recovery and complete unchanged SIM')


if __name__ == '__main__':
    main()
