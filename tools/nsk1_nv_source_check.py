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
CODE = {0x85a98: 'b5f01c0e2200703278022a00d031',
        0x85a44: 'b53078032b20db012320e0012b00d01b',
        0x49c82: 'a905a801c80cc10cc80cc10c'}


def check_lifecycle(text):
    rows = re.findall(r'nsk1_nv_lifecycle: class=([0-9a-f]+) index=([0-9a-f]+) '
        r'digit=0010ce88 digit_first=00 other=0010ce88 other_first=00 '
        r'caller=0024b411 sp=0010ce88 t=([0-9]+\.[0-9]+) observer_writes=0', text)
    expected = [(str(kind), str(index)) for kind,count in ((3,8),(4,5),(5,5)) for index in range(count)]
    if [(kind,index) for kind,index,_ in rows] != expected or text.count('nsk1_nv_lifecycle:') != 18:
        raise ValueError('indexed empty-field lifecycle differs')
    selectors = re.findall(r'nsk1_nv_selector: input=05e7 stage=01 flags=20 caller=0028db52 '
                           r't=([0-9]+\.[0-9]+) observer_writes=0', text)
    clears = re.findall(r'nsk1_nv_clear: caller=00270ead r0=00000000 r4=002a968c '
                        r'r5=0010fd0a r6=000005e7 sp=0010cec0 '
                        r't=([0-9]+\.[0-9]+) observer_writes=0', text)
    if len(selectors) != 1 or len(clears) != 1 or text.count('nsk1_nv_clear:') != 1:
        raise ValueError('accepted-code record-update selection differs')
    if not (float(selectors[0]) < float(clears[0]) < float(rows[0][2])):
        raise ValueError('selector/clear/write order differs')
    times = [float(row[2]) for row in rows]
    if any(a >= b for a,b in zip(times,times[1:])):
        raise ValueError('record update order differs')
    return len(rows)


def check_empty_encoding(text, image, before, after):
    for offset, encoded in CODE.items():
        expected = bytes.fromhex(encoded)
        if image[offset:offset+len(expected)] != expected:
            raise ValueError('own encoder/decoder/copy code differs')
    encoders = []
    pattern = re.compile(r'nsk1_nv_encode: source=([0-9a-f]{8}) first=([0-9a-f]{2}) '
                         r'output=([0-9a-f]{8}) lr=([0-9a-f]{8}) '
                         r't=([0-9]+\.[0-9]+) observer_writes=0')
    for line in text.splitlines():
        if 'nsk1_nv_encode:' not in line:
            continue
        match = pattern.search(line)
        if not match or match.end() != len(line):
            raise ValueError('malformed encoding observation')
        source, first, output, caller, time = match.groups()
        if caller == '00249c51':
            if (source, first, output) != ('0010ce88', '00', '0010ce28'):
                raise ValueError('record encoder input is not the observed empty string')
            encoders.append(float(time))
    writes = [row for row in PATTERN.finditer(text) if 0x550 <= int(row[1],16) < 0x7a2]
    if len(encoders) != 18 or len(writes) != 18:
        raise ValueError('complete empty-input/write sequence required')
    for index, (time, write) in enumerate(zip(encoders, writes)):
        if time >= float(write[6]) or (index and time <= float(writes[index-1][6])):
            raise ValueError('encoding/write order differs')
    if len(before) != 2048 or len(after) != 2048:
        raise ValueError('complete before/after EEPROM required')
    inactive = set()
    for address in range(0x550, 0x7a2, 33):
        if before[address] != 0 or after[address] != 0:
            raise ValueError('nonempty encoded field')
        inactive.update(range(address+1, address+16))
    changed = {index for index,(a,b) in enumerate(zip(before,after)) if a != b}
    if len(changed) != 108 or not changed <= inactive:
        raise ValueError('changes are not confined to the observed inactive digit payloads')
    return len(changed)


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
    parser.add_argument('--empty-encoding', action='store_true')
    parser.add_argument('--lifecycle', action='store_true')
    args = parser.parse_args()
    try:
        count = check_sources((args.run/'error.log').read_text(),
                              (args.run/'nvram/nsk1r4t/eeprom').read_bytes())
        if args.empty_encoding:
            check_empty_encoding((args.run/'error.log').read_text(),
                (args.run/'nvram/nsk1r4t/flash').read_bytes(),
                (args.run/'initial_eeprom.bin').read_bytes(),
                (args.run/'nvram/nsk1r4t/eeprom').read_bytes())
        if args.lifecycle:
            check_lifecycle((args.run/'error.log').read_text())
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSK-1 NV source FAIL: {error}\n')
    print(f'NSK-1: {count} complete record writes match pre-serialization RAM; source semantics unresolved')
    if args.empty_encoding:
        print('108 changed bytes are inactive empty-digit padding; not general storage-integrity acceptance')
    if args.lifecycle:
        print('Accepted-code selector invokes the indexed empty-field update; not unique to Phase-2+')


if __name__ == '__main__':
    main()
