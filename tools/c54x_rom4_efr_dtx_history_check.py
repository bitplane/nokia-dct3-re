"""Check original first-speech history transitions, not codec conformance."""
import argparse
from pathlib import Path
import re

ROW = re.compile(r'efr_dtx_history: pc=([0-9a-f]{4}) flags=([0-9a-f]{4}) history=([0-9a-f]{4}) state=([0-9a-f]{4}) st0=([0-9a-f]{4}) a=([0-9a-f]+)')


def check_trace(text, frames=518):
    groups = []
    for line in text.splitlines():
        if not line.startswith('efr_dtx_history:'):
            continue
        match = ROW.fullmatch(line)
        if not match:
            raise ValueError('malformed DTX history observation')
        row = tuple(int(value, 16) for value in match.groups())
        if row[0] == 0xc78f:
            groups.append([])
        if not groups:
            raise ValueError('history observation precedes decision')
        groups[-1].append(row)
    if len(groups) != frames * 2:
        raise ValueError('missing original/replay history decisions')
    writes = 0
    for index in range(0, len(groups), 2):
        group = groups[index]
        if group != groups[index + 1]:
            raise ValueError('history decision differs after restoration')
        entry, end = group[0], group[-1]
        selected = bool(entry[1] & 0x0400)
        expected = [0xc78f, 0xc793, 0xc79b, 0xc79d] if selected else [0xc78f, 0xc79d]
        if [row[0] for row in group] != expected:
            raise ValueError('first-speech branch differs')
        if any(row[4] & 0x1ff != 0x13 for row in group):
            raise ValueError('direct operand no longer addresses 09c0')
        if any(row[1] != entry[1] for row in group):
            raise ValueError('flags changed within history decision')
        if selected:
            value = int(bool(entry[1] & 0x0100))
            if group[2][5] != value or end[2] != value or end[3] != 5:
                raise ValueError('first-speech history/state publication differs')
            writes += 1
        elif end[2:4] != entry[2:4]:
            raise ValueError('unselected history branch changed state')
    return writes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    try:
        writes = check_trace(args.log.read_text())
    except ValueError as error:
        parser.exit(1, f'FAIL: {error}\n')
    print(f'PASS: 518 original/replay frames; first-speech writes={writes}; no codec conformance claim')


if __name__ == '__main__':
    main()
