"""Distinguish NSB-7 radio-status source text from LCD pixel corruption."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb7_radio_check import contract
from tools.nsb7_menu_check import check_trace, FRAMES


def check(text):
    check_trace(text)
    rows = re.findall(r'nsb7_display_text: font=00341420 pointer=001040e0 '
                      r'words=([0-9a-f]+) t=([0-9.]+) observer_writes=0', text)
    decoded = []
    for words, time in rows:
        if len(words) % 4:
            raise ValueError('truncated UTF-16 source text')
        value = bytes.fromhex(words).decode('utf-16-be')
        if 19 <= float(time) < 26:
            decoded.append(value)
    if not any(re.fullmatch(r' 600   0 xxx\n 0  0 x xxxx\n   0       0\n    CCCH', value)
               for value in decoded):
        raise ValueError('missing complete firmware-generated radio-status source text')


def main():
    from PIL import Image
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('run', type=Path)
    args = parser.parse_args()
    try:
        image = args.image.read_bytes()
        contract(image)
        if image[0x363c0:0x363c5] != b'CCCH\0':
            raise ValueError('own ROM status label differs')
        check((args.run / 'error.log').read_text())
        for name, expected in FRAMES.items():
            with Image.open(args.run / 'snap' / name) as frame:
                if frame.size != (84, 48) or hashlib.sha256(frame.convert('RGB').tobytes()).hexdigest() != expected:
                    raise ValueError('menu control frame differs: ' + name)
    except (OSError, ValueError, UnicodeError) as error:
        parser.exit(1, f'NSB-7 display source: FAIL: {error}\n')
    print('PASS: firmware supplies the radio-status text; physical menu control frames remain correct')


if __name__ == '__main__':
    main()
