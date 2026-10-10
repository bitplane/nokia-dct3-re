#!/usr/bin/env python3
"""Check bounded native scan-filter observations, not RF acquisition."""
import argparse
from pathlib import Path
import re
import sys


COEFFICIENTS = "0387,11a3,2ad6,4000,4000,2ad6,11a3,0387"
STATE = re.compile(r"rom4_scan_filter: t=([0-9.]+) st0=([0-9a-f]{4}) st1=([0-9a-f]{4}) coefficients=([0-9a-f,]+)")
OPERAND = re.compile(r"rom4_scan_operand: index=(\d+) pc=([0-9a-f]{4}) address=([0-9a-f]{4}) value=([0-9a-f]{4})")
HISTORY = (0x84, 0x85, 0x87, 0x86, 0x88, 0x89, 0x8b, 0x8a,
           0x8c, 0x8d, 0x8f, 0x8e, 0x80, 0x81, 0x83, 0x82)
PCS = (0x330d, 0x330e, 0x330f, 0x3310, 0x3313, 0x3314,
       0x3315, 0x3316, 0x3319, 0x331a, 0x331b, 0x331c,
       0x331f, 0x3320, 0x3321, 0x3322)


def check(text):
    states = STATE.findall(text)
    if len(states) != 2:
        raise ValueError("expected two bounded filter-state captures")
    for time, st0, st1, coefficients in states:
        if float(time) <= 0 or st0 != "181f" or st1 != "e900" or coefficients != COEFFICIENTS:
            raise ValueError("filter arithmetic state or coefficients changed")
    operands = OPERAND.findall(text)
    if len(operands) != 32:
        raise ValueError("expected 32 bounded firmware operand reads")
    expected = []
    for index, (pc, history) in enumerate(zip(PCS, HISTORY)):
        if index != 15:
            expected.append((pc, 0x21a9 + index // 2))
        expected.append((pc, history))
    expected.append((0x3323, 0x84))
    for number, (observed, pair) in enumerate(zip(operands, expected), 1):
        index, pc, address, value = observed
        if int(index) != number or (int(pc, 16), int(address, 16)) != pair:
            raise ValueError(f"operand order changed at read {number}")
        if pair[1] >= 0x21a9:
            coefficient = COEFFICIENTS.split(",")[pair[1] - 0x21a9]
            if value != coefficient:
                raise ValueError(f"coefficient value changed at read {number}")
    return len(operands)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        reads = check(args.log.read_text())
    except (OSError, ValueError) as error:
        print(f"ROM4 scan filter: FAIL: {error}", file=sys.stderr)
        return 1
    print(f"ROM4 scan filter: PASS operand_reads={reads} physical_iq_claim=0 acquisition_claim=0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
