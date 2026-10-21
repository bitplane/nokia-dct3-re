#!/usr/bin/env python3
"""Check the bounded NSB-7 MCU failure presentation, not working-phone boot."""
import argparse
import hashlib
from pathlib import Path
import re
from PIL import Image

IMAGE_SHA1 = '4dacc50f57fe295168a53d3e5855a81acf35ab96'
FRAME_SHA256 = 'ed09afeeadae383b77840654453b183a05e10b2cad5ea5757e8c803cbd6b674b'
SAMPLE = re.compile(r'nsb7_boot: pc=([0-9a-f]{8}) lr=([0-9a-f]{8}) cpsr=([0-9a-f]{8}) '
                    r'reset=([0-9a-f]{2}) ctrl=([0-9a-f]{2}) gensio=([0-9a-f]{2}) '
                    r'shared=([0-9a-f,]+) t=([0-9.]+) observer_writes=0')


def check_trace(text):
    if re.findall(r'nsb7_profile: machine=(\w+) observer_writes=0', text) != ['noki8290']:
        raise ValueError('requires the normal fail-closed NSB-7 profile')
    rows = SAMPLE.findall(text)
    if len(rows) != 5 or len(rows) != text.count('nsb7_boot:'):
        raise ValueError('bounded observer inventory differs')
    if [float(row[7]) for row in rows] != [0.25, 1, 3, 5, 6]:
        raise ValueError('sample times differ')
    for row in rows:
        if row[6] != '0000,ffff,0001,0001':
            raise ValueError('uncompleted shared state differs')
    for row in rows[1:]:
        if int(row[0], 16) not in range(0x2f82fe, 0x2f8306, 2) or row[4] != '05':
            raise ValueError('supervisor continuation differs')
    if 'LUA ERROR' in text or 'c54x_failure:' in text:
        raise ValueError('runtime observer or CPU failed')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('run', type=Path)
    args = parser.parse_args()
    try:
        if hashlib.sha1(args.image.read_bytes()).hexdigest() != IMAGE_SHA1:
            raise ValueError('requires qualified own NSB-7 image')
        check_trace((args.run / 'error.log').read_text())
        frame = Image.open(args.run / 'snap/nsb7_boot_6.png').convert('RGB')
        if frame.size != (84, 48) or hashlib.sha256(frame.tobytes()).hexdigest() != FRAME_SHA256:
            raise ValueError('CONTACT SERVICE frame differs')
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 baseline: FAIL: {error}\n')
    print('PASS: own NSB-7 MCU failure presentation; no idle/input/DSP acceptance')


if __name__ == '__main__':
    main()
