"""Verify NSB-6 firmware-owned A/123 save or preserved cold readback."""
import argparse
from hashlib import sha256
from pathlib import Path
import sys

from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.sim_phonebook_check import validate_phonebook, validate_phonebook_storage


def verify(stage, text, data):
    validate_phonebook_storage(data, b'A')
    if '[LUA ERROR]' in text:
        raise ValueError('input fixture error')
    if stage == 'save':
        validate_phonebook(text, data, b'A')
        marker = '8890_phonebook_physical: action=save'
    else:
        if 'ins=dc' in text:
            raise ValueError('cold readback rewrote SIM storage')
        if 'ins=b2 p1=01 p2=04 p3=20 selected=6f3a' not in text:
            raise ValueError('cold boot did not read ADN record 1')
        marker = '8890_phonebook_read_physical: action=contact'
    position = text.find(marker)
    if position < 0 or '8890_keypad_decoded: key=19' not in text[position:]:
        raise ValueError('missing physical Save/Detail decode')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stage', choices=('save', 'readback'))
    parser.add_argument('trace', type=Path)
    parser.add_argument('nvram', type=Path)
    parser.add_argument('frame', type=Path)
    args = parser.parse_args()
    try:
        verify(args.stage, args.trace.read_text(errors='replace'), args.nvram.read_bytes())
        with Image.open(args.frame) as frame:
            expected = {'save': '6664b418c08d611027e09b37318f3642f67c2275630644ed0b011df11c2ee617',
                        'readback': '5bd54a4d4956eab187335f0c8e092a56b2613ad17e3358f1dfb9236c6fa6ea69'}[args.stage]
            if frame.size != (84, 48) or sha256(frame.convert('L').crop((0, 0, 84, 36)).tobytes()).hexdigest() != expected:
                raise ValueError('reviewed Save/123 detail pixels differ')
    except (OSError, ValueError) as error:
        parser.exit(1, f'8890 phonebook FAIL: {error}\n')
    print(f'8890 phonebook {args.stage} PASS: exact A/123 and physical UI')
