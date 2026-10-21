"""Check physical NSB-7 menu navigation after own fallback-code acceptance."""
import argparse
import hashlib
from pathlib import Path
import re
import sys
from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb7_security_check import check_trace as check_security
from tools.nsb7_input_check import IMAGE_SHA1

FRAMES = {
    'nsb7_menu_26.png': '76728306449ad0f2ae8cbdbd1fc5e880d3dcff054c955e8b4a2aabc87db8e7e4',
    'nsb7_menu_29.png': 'bbe75081a8d80930224fc1cce32498db06fc7f5e1e46f037fdca00cf549fed8b',
    'nsb7_menu_32.png': '095da94a00608cfa2ab979e78323dcb85961ac808ad0c48ae0fdcd63f612c750',
}


def check_trace(text):
    check_security(text)
    keys = [('Left Softkey / Menu', 0x19), ('Scroll Down', 0x18), ('Left Softkey / Menu', 0x19)]
    rows = list(re.finditer(r'nsb7_menu_input: key=(.*?) state=([01]) ', text))
    if [(row[1], row[2]) for row in rows] != [(key, state) for key, _ in keys for state in ('1', '0')]:
        raise ValueError('physical menu contact inventory/order differs')
    for index, (_, decoded) in enumerate(keys):
        held = text[rows[index*2].end():rows[index*2+1].start()]
        if f'nsb7_key_decode: key={decoded:02x} observer_writes=0' not in held:
            raise ValueError('menu contact did not decode')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('run', type=Path)
    args = parser.parse_args()
    try:
        if hashlib.sha1(args.image.read_bytes()).hexdigest() != IMAGE_SHA1:
            raise ValueError('requires own qualified image')
        check_trace((args.run / 'error.log').read_text())
        for name, expected in FRAMES.items():
            frame = Image.open(args.run / 'snap' / name).convert('RGB')
            if frame.size != (84, 48) or hashlib.sha256(frame.tobytes()).hexdigest() != expected:
                raise ValueError('menu presentation differs: ' + name)
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 menu: FAIL: {error}\n')
    print('PASS: own HLE physical Messages/Call log/Missed calls navigation; usable idle/RF unproved')


if __name__ == '__main__':
    main()
