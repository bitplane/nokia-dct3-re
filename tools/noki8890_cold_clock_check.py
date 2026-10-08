"""Check own-ROM retained clock validation and reviewed 13:47 cold idle."""
import argparse
import hashlib
from pathlib import Path
import re

from PIL import Image


def verify(text, stored):
    if len(stored) != 9 or stored[1:3] != bytes((47, 13)):
        raise ValueError('physical fixture did not store running 13:47')
    if stored[4:] != bytes((0, 30, 11, 0x50, 1)):
        raise ValueError('retained controller snapshot mismatch')
    if 'LUA ERROR' in text or 'LUA error' in text:
        raise ValueError('runtime Lua failure')
    if not re.search(r'8890_clock_nv_result: result=00000001 flags=00\b', text):
        raise ValueError('own clock record was not restored')
    if not re.search(r'kind=app_write pc=002dffdc address=00137414 '
                     r'data=2a000000 mask=ff000000\b', text):
        raise ValueError('firmware did not select valid retained clock state')
    if '8890_clock_physical:' in text:
        raise ValueError('cold boot supplied time/date keys')


def verify_minute(text):
    if not re.search(r'ccont_rtc: event=second time=13:47:59 .*?t=59\.000000000'
                     r'.*?ccont_rtc: event=second time=13:48:00 .*?status=33 mask=50 t=60\.000000000'
                     r'.*?ccont_rtc: event=second time=13:48:01 .*?status=13 mask=50 t=61\.000000000'
                     r'.*?8890_clock_cold: t=90\b', text, re.S):
        raise ValueError('missing ordered retained-clock minute rollover and serviced minute source')


def check_frame(path, *, advanced=False):
    with Image.open(path) as image:
        if image.size != (84, 48):
            raise ValueError('unexpected LCD geometry')
        digest = hashlib.sha256(image.convert('L').tobytes()).hexdigest()
        # Full frame includes the distinct 13:47 clock, not only the idle body.
        expected = ('e40423cea2d28b4aebf15cf10c650216ed2137f1caacfd294dcc31e856b90524'
                    if advanced else 'a3f1e6eb6f5ef68a282dea0a914141dfef4423f2397b387b1828f96e52e74efc')
        if digest != expected:
            raise ValueError('reviewed cold idle clock frame mismatch')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('seed_nvram', type=Path)
    parser.add_argument('cold_directory', type=Path)
    args = parser.parse_args()
    try:
        verify((args.cold_directory / 'error.log').read_text(errors='replace'),
               args.seed_nvram.read_bytes())
        check_frame(args.cold_directory / 'snap/8890_clock_cold_30.png')
    except (OSError, ValueError) as error:
        parser.exit(1, f'8890 cold clock FAIL: {error}\n')
    print('8890 own-record cold 13:47 idle PASS; offline calendar advance unproved')


if __name__ == '__main__':
    main()
