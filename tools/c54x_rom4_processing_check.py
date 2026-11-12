#!/usr/bin/env python3
"""Check executed ROM4 no-cell processing, not speculative fetch counts."""

import argparse
from collections import Counter
from pathlib import Path
import re
import sys

try:
    from tools.c54x_rom4_rf_boundary_check import check as check_rf
except ModuleNotFoundError:
    from c54x_rom4_rf_boundary_check import check as check_rf


FIELDS = ('address word fd mode flags candidate control2180 control21c2 '
          'pair94 pair96 a b st1 bk ar0 ar3 ar4 ar5 ar6').split()
WIDTHS = [4] * 8 + [8, 8, 10, 10] + [4] * 7
RECORD = re.compile('rom4_processing_execute: ' + ' '.join(
    name + rf'=([0-9a-f]{{{width}}})' for name, width in zip(FIELDS, WIDTHS)))
WORDS = {0x32f4: 0xf074, 0x330a: 0x74d6, 0x332b: 0x4592,
         0x3347: 0x4592, 0x3357: 0x7719, 0x3360: 0xf074, 0x2400: 0xfc00,
         0x3362: 0x7713, 0x336c: 0xf074, 0x2402: 0xfc00, 0x336e: 0x7719,
         0x3382: 0x56f8, 0x3385: 0xfa43, 0x33ac: 0xf274}
COEFFICIENTS = '0387,11a3,2ad6,4000,4000,2ad6,11a3,0387'


def check(text, *, scan_30s=False):
    if not scan_30s:
        raise ValueError('processing acceptance requires the current 30-second scan')
    rf = check_rf(text, scan_30s=True)
    if text.count('rom4_processing_observer: schema=1 phase=instruction_entry observer_writes=0') != 1:
        raise ValueError('missing unique execution-entry observer')
    matches = list(RECORD.finditer(text))
    if len(matches) != text.count('rom4_processing_execute:'):
        raise ValueError('malformed execution-entry record')
    groups = []
    for match in matches:
        row = dict(zip(FIELDS, (int(value, 16) for value in match.groups())))
        address = row['address']
        if WORDS.get(address) != row['word'] or row['mode'] != 1:
            raise ValueError('unexpected executed instruction or mode')
        if address == 0x32f4:
            groups.append([])
        if not groups:
            raise ValueError('processing entry missing before executed work')
        groups[-1].append(row)
        if (row['control2180'], row['control21c2'], row['pair94'], row['pair96']) != (0x5a, 0xf4cf, 0x3d9de, 0xfa0):
            raise ValueError('processing control or full-width operands changed')
        if address in (0x2400, 0x2402, 0x3385):
            expected_a = 0xfa0 if address == 0x3385 else 0
            if (row['a'], row['b']) != (expected_a, 0):
                raise ValueError('hook/comparison accumulator changed')
    if len(groups) != 1028:
        raise ValueError('30-second executed mode-1 snapshot changed')
    counts = Counter(row['address'] for group in groups for row in group)
    for group in groups:
        early = group[0]['fd'] == 0
        if group[0]['fd'] not in (0, 1):
            raise ValueError('unknown reduction selector')
        prefix = [0x32f4] + [0x330a] * 8
        prefix += [0x332b] if early else [0x3347, 0x3357]
        prefix += [0x3360, 0x2400, 0x3362, 0x336c, 0x2402, 0x336e]
        addresses = [row['address'] for row in group]
        if addresses[:len(prefix)] != prefix or addresses[len(prefix):] not in (
                [], [0x3382, 0x3385], [0x3382, 0x3385, 0x33ac]):
            raise ValueError('executed filter/call/return/branch ordering changed')
        for index, row in enumerate(group[1:9]):
            # Read-stage address updates are already visible at Execute entry.
            expected = (0x10, 2, 0x1a48 + 2 * index, 0x80 + ((7 + 4 * index) & 15),
                        0x21aa, 0x80 + ((1 + 4 * index) & 15))
            if tuple(row[key] for key in ('bk', 'ar0', 'ar3', 'ar4', 'ar5', 'ar6')) != expected:
                raise ValueError('executed filter addressing changed')
    if any(row['st1'] != 0xc900 for row in groups[0][1:5]):
        raise ValueError('initial executed filter status changed')
    if (counts[0x332b], counts[0x3347], counts[0x3385], counts[0x33ac]) != (22, 1006, 514, 4):
        raise ValueError('executed reduction/continuation snapshot changed')
    coefficients = re.findall(r'rom4_processing_coefficients: words=(\S+)', text)
    if coefficients != [COEFFICIENTS] * counts[0x330a]:
        raise ValueError('missing or changed executed filter coefficients')
    continuations = [row for group in groups for row in group if row['address'] == 0x33ac]
    expected = [(fd, flags, candidate, (candidate + 6) << 16, 0x5a0000)
                for fd, flags in ((0, 0x83a), (1, 0x832)) for candidate in (0x27, 0x2d)]
    if [tuple(row[key] for key in ('fd', 'flags', 'candidate', 'a', 'b')) for row in continuations] != expected:
        raise ValueError('executed continuation operands/order changed')
    writes = list(re.finditer(r'rom4_comparison_write: .*address=06fd data=([0-9a-f]{4})', text))
    first = next(match for match in matches if match[1] == '32f4')
    first_steady = next(match for match in matches if match[1] == '3347')
    if ([match[1] for match in writes] != ['0000', '0000', '0001'] or
            not writes[0].start() < first.start() < writes[1].start() < writes[2].start() < first_steady.start()):
        raise ValueError('reduction selector write lifecycle changed')
    inputs = re.findall(r'rom4_comparison_input: reads=(\d+) nonzero=(\d+) reduction_nonzero=(\d+)', text)
    if inputs != [(str(rf['rf_reads']), '0', '0')] or 'Lua error' in text:
        raise ValueError('unattached sample/reduction census changed or Lua failed')
    return len(groups)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--scan-30s', action='store_true')
    args = parser.parse_args()
    try:
        frames = check(args.log.read_text(errors='replace'), scan_30s=args.scan_30s)
    except (OSError, ValueError) as error:
        print(f'ROM4 no-cell processing rejected: {error}', file=sys.stderr)
        return 1
    print(f'ROM4 executed no-cell processing: PASS mode1={frames} filter_iterations={frames * 8} physical_rf_claim=0')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
