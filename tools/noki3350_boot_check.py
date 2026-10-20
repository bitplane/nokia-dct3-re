#!/usr/bin/env python3
"""Check the fail-closed own-MCU NHM-9 upload boundary, not phone boot."""

import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.extract_nsm3_verifier import extract


ARGUMENTS = '0100030000198000000100010200'


def check(text, program):
    if len(program) != 494:
        raise ValueError('requires the 247-word own program')
    if re.search(r'\[LUA ERROR\]|Error in|Fatal error', text):
        raise ValueError('observer or machine failed')
    if not re.search(r'nhm9_boot_initial_storage: first=00530000 bytes=851968 non_erased_words=0 observer_writes=0', text):
        raise ValueError('initial storage is not erased')
    if not re.search(r'nhm9_boot_flash_summary: writes=200 observer_writes=0', text):
        raise ValueError('firmware storage writes differ')
    uploads = re.findall(r'nhm9_boot_program: words=([0-9a-f]+) observer_writes=0', text)
    if len(uploads) != 1 or bytes.fromhex(uploads[0]) != program:
        raise ValueError('installed own program differs')
    releases = re.findall(r'nhm9_boot_release: arguments=([0-9a-f]+) pc=([0-9a-f]+) t=([0-9.]+) observer_writes=0', text)
    if len(releases) != 1 or releases[0][0] != ARGUMENTS or int(releases[0][1], 16) != 0x31dc3a:
        raise ValueError('release arguments differ')
    if not 0 < float(releases[0][2]) < 0.5:
        raise ValueError('release timing outside observed startup interval')
    if not re.search(r'nhm9_boot_catalogue: address=00120f8c entries=28 observer_writes=0', text):
        raise ValueError('relocated own catalogue absent')
    if not re.search(r'nhm9_boot_catalogue_summary: matches=1 aligned_ram_bytes=262144 observer_writes=0', text):
        raise ValueError('catalogue coverage differs')
    samples = re.findall(r'nhm9_boot_sample: t=([0-9.]+) pc=([0-9a-f]+) sp=([0-9a-f]+) '
                         r'shared=([0-9a-f]+) writes=(\d+) ctrl=([0-9a-f]+) dsp_reset=([0-9a-f]+) observer_writes=0', text)
    if [float(row[0]) for row in samples] != [0.1, 0.5, 1, 2, 5, 10]:
        raise ValueError('bounded observation interval differs')
    for row in samples[1:]:
        _, pc, sp, shared, writes, control, reset = row
        if (int(pc, 16) not in (0x31dc82, 0x31dc84, 0x31dc86)
                or int(sp, 16) != 0x12bca8 or int(writes) != 543
                or shared != '0000ffff0000000000ea00ea000000f0'
                or control != '05' or reset != '01'):
            raise ValueError('first ownership wait differs')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('flash', type=Path)
    parser.add_argument('run', type=Path)
    args = parser.parse_args()
    try:
        image = args.flash.read_bytes()
        check((args.run / 'error.log').read_text(), extract(image, '3350'))
        persisted = (args.run / 'nvram/noki3350/flash').read_bytes()
        if len(persisted) != 0x400000 or persisted[:len(image)] != image:
            raise ValueError('unmodified executable boundary differs')
        if persisted[0x330006:0x33000a] != b'PMM\0' or persisted[0x3e0006:0x3e000c] != b'EEPROM':
            raise ValueError('firmware-created storage catalogue absent')
    except (OSError, ValueError) as error:
        parser.exit(1, f'NHM-9 MCU boundary: FAIL: {error}\n')
    print('NHM-9 MCU boundary: PASS own upload, arguments, catalogue and first ownership wait; no boot/native claim')


if __name__ == '__main__':
    main()
