#!/usr/bin/env python3
"""Verify NSB-6 physical A/5551234 SMS submission and transport closure."""
import argparse
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.noki8850_outgoing_sms_check import verify as verify_submission
from tools.noki8890_registration_check import verify as verify_registration


def verify(text, *, pcs1900=False):
    if pcs1900:
        verify_registration(text, pcs1900=True)
    verify_submission(text, product='8890', key_separator=':')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--pcs1900', action='store_true')
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors='replace'), pcs1900=args.pcs1900)
    except (OSError, ValueError) as error:
        print(f'FAIL - {error}', file=sys.stderr)
        return 1
    print('8890 outgoing SMS PASS: physical A/5551234, CP/RP closure and paging')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
