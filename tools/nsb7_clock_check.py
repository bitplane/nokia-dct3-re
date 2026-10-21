"""Qualify own NSB-7 time setting and RTC-backed versus flash-only restart."""
import argparse
import hashlib
from pathlib import Path
import re
import sys
from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb7_security_check import check_trace as check_security
from tools.nsb7_storage_check import inspect


def check_counter(data, retained):
    if len(data) != 9 or data[1:3] != (bytes((47, 13)) if retained else bytes((0, 12))):
        raise ValueError('RTC minute/hour retention differs')


def check_setting(text):
    check_security(text)
    rows = list(re.finditer(r'nsb7_menu_input: key=(.*?) state=([01]) t=([0-9.]+)', text))
    expected = [('Keypad 1', 53, 1), ('Keypad 3', 56, 3), ('Keypad 4', 59, 4),
                ('Keypad 7', 62, 7), ('Left Softkey / Menu', 65, 0x19)]
    for key, when, decoded in expected:
        press = [row for row in rows if row[1] == key and row[2] == '1' and float(row[3]) == when]
        release = [row for row in rows if row[1] == key and row[2] == '0' and float(row[3]) == when + 0.2]
        if len(press) != 1 or len(release) != 1:
            raise ValueError('physical time-setting contact absent')
        if f'nsb7_key_decode: key={decoded:02x} observer_writes=0' not in text[press[0].end():release[0].start()]:
            raise ValueError('time-setting key did not decode')


def frame(path, expected, crop=None):
    pixels = Image.open(path).convert('RGB')
    if pixels.size != (84, 48):
        raise ValueError('LCD geometry differs')
    if crop:
        pixels = pixels.crop(crop)
    if hashlib.sha256(pixels.tobytes()).hexdigest() != expected:
        raise ValueError('clock presentation differs: ' + str(path))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('setting', 'retained', 'flash_only'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    try:
        for run in (args.setting, args.retained, args.flash_only):
            check_security((run / 'error.log').read_text())
            inspect((run / 'nvram/nsb7hle/flash').read_bytes())
        check_setting((args.setting / 'error.log').read_text())
        for run, retained in ((args.setting, True), (args.retained, True), (args.flash_only, False)):
            check_counter((run / 'nvram/nsb7hle/ccont').read_bytes(), retained)
        frame(args.setting / 'snap/nsb7_menu_62.png', '6eac86dbec34440a4ba4a0a51edbb455a5cc5d8d985255bec6a5d407eb4838b3')
        frame(args.setting / 'snap/nsb7_menu_65.png', '8c2311d07ffd160e6dd9ef6499f904496410d1564052dc110714dd3e945a2e0a')
        frame(args.flash_only / 'snap/nsb7_security_25.png', '0d34f33bcd813eebbf9fb250eae41165b7e6ff8c1f4d4a80b861c9344b847fa3')
        # Only the top-right 13:47 clock is qualified; main-area idle content is not.
        frame(args.retained / 'snap/nsb7_security_25.png', '0e11115fdca68eb7f5f54e278a888989711d31bf980edaabf925441c64b706db', (52, 0, 78, 8))
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 clock: FAIL: {error}\n')
    print('PASS: physical 13:47 setting; own CCONT-backed retention; flash-only time prompt; idle content unproved')


if __name__ == '__main__':
    main()
