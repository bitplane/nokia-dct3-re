"""Verify physical NSM-3 A/5551234 submission using shared GSM checks."""
import argparse
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.noki8850_outgoing_sms_check import verify as verify_submission


def verify(text):
    if '[LUA ERROR]' in text:
        raise ValueError('fixture error')
    verify_submission(text, product='8210', key_separator=':', allow_message_reference=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    try:
        with args.log.open(errors='replace') as stream:
            text = ''.join(line for line in stream if 'GSM service' in line or
                           'gsm_sms_submit:' in line or 'LAPDm' in line or
                           'PCH no-identity' in line or '8210_sms_send_physical' in line or
                           '8210_keypad_decoded' in line or '[LUA ERROR]' in line)
        verify(text)
    except (OSError, ValueError) as error:
        parser.exit(1, f'8210 outgoing SMS FAIL: {error}\n')
    print('8210 physical A/5551234 SMS submission and CP/RP/RR closure PASS')
