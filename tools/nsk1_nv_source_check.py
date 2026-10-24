"""Compare observed NSK-1 EEPROM write buffers with complete saved records.

This localizes the known Toolkit rewrite before serialization. It does not
validate record contents or explain how firmware populated the source buffer.
"""
import argparse
from pathlib import Path
import re

PATTERN = re.compile(
    r'nsk1_nv_source: address=([0-9a-f]{4}) pointer=([0-9a-f]{8}) '
    r'length=(\d+) caller=([0-9a-f]{8}) data=([0-9a-f]*) '
    r't=([0-9]+\.[0-9]+) observer_writes=0')


def check_sources(text, bank):
    if len(bank) != 2048:
        raise ValueError('complete EEPROM required')
    rows = []
    for line in text.splitlines():
        if 'nsk1_nv_source:' not in line:
            continue
        match = PATTERN.search(line)
        if not match or match.end() != len(line):
            raise ValueError('malformed source observation')
        address, pointer, length, caller, data, time = match.groups()
        address, length = int(address, 16), int(length)
        if 0x550 <= address < 0x7a2:
            payload = bytes.fromhex(data)
            if (pointer != '0010ce38' or caller != '0028ca0f' or length != 33
                    or len(payload) != length or payload != bank[address:address+length]):
                raise ValueError('record source differs from persisted bytes')
            rows.append((address, float(time)))
    if [address for address, _ in rows] != [0x550+i*33 for i in range(18)]:
        raise ValueError('expected exactly eighteen ordered record writes')
    if any(a[1] >= b[1] for a, b in zip(rows, rows[1:])):
        raise ValueError('record write times are not increasing')
    return len(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    args = parser.parse_args()
    try:
        count = check_sources((args.run/'error.log').read_text(),
                              (args.run/'nvram/nsk1r4t/eeprom').read_bytes())
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSK-1 NV source FAIL: {error}\n')
    print(f'NSK-1: {count} complete record writes match pre-serialization RAM; source semantics unresolved')


if __name__ == '__main__':
    main()
