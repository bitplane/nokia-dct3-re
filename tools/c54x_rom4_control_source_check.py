"""Check the bounded fresh-profile ROM4 control-source capture, not RF units."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.c54x_rom4_rf_boundary_check import check as check_boundary


def check(text):
    boundary = check_boundary(text)
    tables = re.findall(
        r'rom4_rf_control_table: hit=(\d+) base=([0-9a-f]{4}) words=([0-9a-f,]+) '
        r'pointer197b=([0-9a-f]{4}) pointer197d=([0-9a-f]{4}) setup=([0-9a-f,]+)', text)
    expected = [('1', '1920', '2a04,0006,0041,0040,27a2,0030,0041,0020',
                 '1914', '1920', '76f8,197b,1914,76f8,197d,1920,fc00'),
                ('2', '1920', '2a04,0006,0041,0040,27a2,0030,0041,0020',
                 '1914', '1920', '76f8,197b,1914,76f8,197d,1920,fc00')]
    if tables != expected:
        raise ValueError('missing, duplicated or changed native control table/setup')
    accumulators = re.findall(
        r'rom4_rf_control_accumulator: a=([0-9a-f]{10}) b=([0-9a-f]{10}) '
        r'al=([0-9a-f]{4}) ah=([0-9a-f]{4}) routine=([0-9a-f,]+)', text)
    if len(accumulators) != 1:
        raise ValueError('expected one accumulator control observation')
    a, b, low, high, routine = accumulators[0]
    if (a, b, low, high) != ('0000302813', '0000302813', '2813', '0030'):
        raise ValueError('accumulator halves do not match native port publication')
    if '75f8,0008,0031,f495,f495,75f8,0009,0032' not in routine:
        raise ValueError('missing accumulator-MMR port-write instructions')
    # The observer's stdout result is not in error.log; count actual records.
    for pc, count in (('a22f', 2), ('a23e', 2), ('4025', 1)):
        hits = re.findall(r'rom4_rf_operand: pc=' + pc + r' hit=(\d+)\b', text)
        if hits != [str(i) for i in range(1, count + 1)]:
            raise ValueError('control observer count changed at ' + pc)
    return boundary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    try:
        check(args.log.read_text(errors='replace'))
    except (OSError, ValueError) as error:
        parser.exit(1, f'ROM4 control source FAIL: {error}\n')
    print('ROM4 control source PASS; acquisition_claim=0 sample_encoding_claim=0')


if __name__ == '__main__':
    main()
