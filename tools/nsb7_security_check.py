"""Qualify physical NSB-7 phone-code acceptance or the erased-retry control."""
import argparse
import hashlib
from pathlib import Path
import re
import sys
from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb7_input_check import IMAGE_SHA1
from tools.nsb7_storage_check import inspect


def check_trace(text, accepted=True):
    for marker in ('nsb7_profile: machine=nsb7hle observer_writes=0',
                   'runtime_hle_handoff pc=2c75 native_suspended=1',
                   'nsb7_security_nv_read: offset=09d4 destination=00130a8c length=8',
                   'nsb7_security_read: pc=00295a40 data=ffffff',
                   'nsb7_security_read: pc=00295a88 data=3132333435 valid=0',
                   'nsb7_security_compare: input=313233343500 encoded=d83b30d8 stored=d83b30d8',
                   'nsb7_security_retry: match=1 retries=' + ('00' if accepted else 'ff')):
        if marker not in text:
            raise ValueError('security evidence absent: ' + marker)
    if any(error in text for error in ('LUA ERROR', 'c54x_failure:', 'observer_writes=1')):
        raise ValueError('runtime observer failure')
    keys = ['Keypad 1', 'Keypad 2', 'Keypad 3', 'Keypad 4', 'Keypad 5', 'Left Softkey / Menu']
    rows = re.findall(r'nsb7_security_input: key=(.*?) state=([01]) ', text)
    if rows != [(key, state) for key in keys for state in ('1', '0')]:
        raise ValueError('physical security input inventory/order differs')
    for key, decoded in zip(keys, (1, 2, 3, 4, 5, 0x19)):
        start = text.index(f'nsb7_security_input: key={key} state=1 ')
        end = text.index(f'nsb7_security_input: key={key} state=0 ')
        if f'nsb7_key_decode: key={decoded:02x} observer_writes=0' not in text[start:end]:
            raise ValueError('physical key did not decode: ' + key)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('run', type=Path)
    parser.add_argument('--rejected', action='store_true')
    args = parser.parse_args()
    try:
        if hashlib.sha1(args.image.read_bytes()).hexdigest() != IMAGE_SHA1:
            raise ValueError('requires own qualified image')
        check_trace((args.run / 'error.log').read_text(), not args.rejected)
        cache, _, _ = inspect((args.run / 'nvram/nsb7hle/flash').read_bytes())
        if cache[0x110:0x113] != b'\xff'*3 or cache[0x9d8] != (0xff if args.rejected else 0):
            raise ValueError('own packed-code/retry storage differs')
        expected = ('460778ecafd4fb9a23ec8c6632d3b4272b29c63c059b8c2eb764ad810f6c53c8' if args.rejected else
                    '5faa6f4b832dbc238c47bb37f847ece5d6b0fbc2f0c30ff7203f519484055099')
        frame = Image.open(args.run / 'snap/nsb7_security_14.png').convert('RGB')
        if frame.size != (84, 48) or hashlib.sha256(frame.tobytes()).hexdigest() != expected:
            raise ValueError('Code accepted/error presentation differs')
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 phone security: FAIL: {error}\n')
    print('PASS: physical fallback-code ' + ('erased-retry rejection' if args.rejected else 'zero-retry acceptance') + '; idle/RF unproved')


if __name__ == '__main__':
    main()
