#!/usr/bin/env python3
"""Check NAM-2 C3 security presentation and independently restarted storage."""

import argparse
import hashlib
from pathlib import Path
import re
import sys

from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.noki2100_v521_security_check import check_storage


FRAMES = {
    'nam2_v521_security_before.png': 'b295728b818021342c6ac64148ac54c3be92f63a588c1607c325e9062935b491',
    'nam2_v521_security_5.png': 'ced78aa7c098ae337521429d1855e17e1f89f91c57cfe5bf0c59f669a098a522',
    'nam2_v521_security_6.png': '7ed84ddca790adea3f96ce87981c3bb78fbdf7c12b21955ab5a490f037baf7f4',
    'nam2_v521_security_after.png': '29c0cbe037a2a5786358f1dc1560fec4733a48303ef60366cf77f6ed795662c4',
}


def check_boot(text):
    if '[LUA ERROR]' in text:
        raise ValueError('observer failed')
    if not re.search(r'flash_family: family=00000003 id0=0000 maker=0089 device=88c2 third=0001', text):
        raise ValueError('C3 family selection absent')
    checks = re.findall(r'nam2_v521_checksum: computed=([0-9a-f]+) stored=([0-9a-f]+)', text)
    if not checks or any(int(a, 16) != 0xa0aa or int(b, 16) != 0xa0aa for a, b in checks):
        raise ValueError('own checksum not validated')
    releases = re.findall(r'nam2_v521_release: result=([0-9a-f]+)', text)
    if not releases or int(releases[0], 16) != 1:
        raise ValueError('startup release failed')


def check_pmm(source, flash):
    if len(flash) == len(source) + 18:
        if flash[-17] & 1:
            raise ValueError('invalid C3 OTP factory-lock state')
        flash = flash[:-18]
    check_storage(source, flash)
    if flash[0x1fc006:0x1fc00c] != b'EEPROM':
        raise ValueError('own EEPROM catalogue lost')
    if sum(flash[0x1fc026:0x1fc142]) != int.from_bytes(flash[0x1fc142:0x1fc146], 'big'):
        raise ValueError('stored identity checksum invalid')


def check_frames(directory):
    for name, expected in FRAMES.items():
        with Image.open(directory / name) as source:
            frame = source.convert('RGB')
        if frame.size != (96, 65) or hashlib.sha256(frame.tobytes()).hexdigest() != expected:
            raise ValueError(f'reviewed frame differs: {name}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('first_run', type=Path)
    parser.add_argument('cold_run', type=Path)
    parser.add_argument('cold_flash', type=Path)
    args = parser.parse_args()
    try:
        image = args.image.read_bytes()
        check_pmm(image, (args.first_run / 'nvram/nam2c3_1/flash').read_bytes())
        check_pmm(image, args.cold_flash.read_bytes())
        first = (args.first_run / 'error.log').read_text()
        check_boot(first)
        check_boot((args.cold_run / 'error.log').read_text())
        actions = re.findall(r'nam2_v521_security_physical: index=(\d+) key=(.*?) t=([0-9.]+)', first)
        expected = [(str(i + 1), key, 8 + i * 0.5) for i, key in enumerate(
            ['Keypad 1', 'Keypad 2', 'Keypad 3', 'Keypad 4', 'Keypad 5', 'Left Softkey / Menu'])]
        if [(i, key, float(t)) for i, key, t in actions] != expected:
            raise ValueError('physical 12345/OK sequence differs')
        check_frames(args.first_run / 'snap')
    except (OSError, ValueError) as error:
        parser.exit(1, f'NAM-2 C3 checkpoint: FAIL: {error}\n')
    print('NAM-2 C3: Code accepted pixels and cold checksum/release verified; usable idle unproved')


if __name__ == '__main__':
    main()
