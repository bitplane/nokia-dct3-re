#!/usr/bin/env python3
"""Verify the NSM-2 physical outgoing A/5551234 SMS transaction."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.radio_call_lifecycle_common import require_ordered

SUBMIT = '390118000100069121436587090d11010781551532f40000a70141'


def verify(text: str, product: str = '8850', key_separator: str = '',
           allow_message_reference: bool = False) -> None:
    # TP-MR is handset-maintained across successful submissions.
    submit = (SUBMIT[:30] + r'[0-9a-f]{2}' + SUBMIT[32:]
              if allow_message_reference else SUBMIT)
    require_ordered(text, (
        ('physical A', re.compile(re.escape(product) + r'_sms_send_physical: action=text_A')),
        ('physical Send', re.compile(re.escape(product) + r'_sms_send_physical: action=confirm_send')),
        ('Send decode', re.compile(re.escape(product + '_keypad_decoded' + key_separator) + r' key=19\b')),
        ('exact SMS-SUBMIT', re.compile(r'GSM service uplink sapi=3 pd=09 message=01 length=27 data=' + submit + r'\b')),
        ('accepted submission', re.compile(r'gsm_sms_submit: cp=39 rp=01 smsc=1234567890 destination=5551234 alphabet=0 user_length=1 outcome=0 status_report=0')),
        ('network CP-ACK', re.compile(r'GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2')),
        ('network RP-ACK', re.compile(r'GSM service downlink kind=18 sapi=3 pd=09 message=01 length=5')),
        ('handset CP-ACK', re.compile(r'GSM service uplink sapi=3 pd=09 message=04 length=2 data=3904')),
        ('RR release', re.compile(r'LAPDm service Channel Release acknowledged')),
        ('return to paging', re.compile(r'PCH no-identity fill')),
    ), product + ' outgoing SMS')
    if text.count('gsm_sms_submit:') != 1:
        raise ValueError('expected one accepted SMS submission')


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors='replace'))
    except (OSError, ValueError) as error:
        print(f'FAIL - {error}', file=sys.stderr)
        return 1
    print('8850 outgoing SMS PASS: physical input, exact A/5551234, CP/RP closure and paging')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
