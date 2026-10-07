"""Verify physical NSM-3 A/5551234 submission using shared GSM checks."""
import argparse
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.noki8850_outgoing_sms_check import verify as verify_submission
from tools.noki8850_outgoing_sms_check import verify_silence, check_recovery


def verify(text, *, rejected=False):
    if '[LUA ERROR]' in text:
        raise ValueError('fixture error')
    verify_submission(text, product='8210', key_separator=':', allow_message_reference=True,
                      rejected=rejected)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    outcome = parser.add_mutually_exclusive_group()
    outcome.add_argument('--rejected', action='store_true')
    outcome.add_argument('--rp-silence', action='store_true')
    parser.add_argument('--recovery-frames', type=Path)
    args = parser.parse_args()
    try:
        with args.log.open(errors='replace') as stream:
            text = ''.join(line for line in stream if 'GSM service' in line or
                           'gsm_sms_submit:' in line or 'LAPDm' in line or
                           'PCH no-identity' in line or '8210_sms_send_physical' in line or
                           'gsm_call_adapter:' in line or 'TX packet' in line or 'RX enqueue' in line or
                           '8210_sms_recovery_physical' in line or
                           '8210_keypad_decoded' in line or '[LUA ERROR]' in line)
        if args.rp_silence:
            verify_silence(text, product='8210')
        else:
            verify(text, rejected=args.rejected)
        if args.recovery_frames:
            if not (args.rejected or args.rp_silence):
                raise ValueError('recovery frames require a failed transaction')
            check_recovery(text, args.recovery_frames, product='8210', key_separator=':',
                           rp_silence=args.rp_silence)
    except (OSError, ValueError) as error:
        parser.exit(1, f'8210 outgoing SMS FAIL: {error}\n')
    print('8210 physical A/5551234 ' +
          ('RP silence' if args.rp_silence else 'RP rejection' if args.rejected else 'SMS submission') +
          ' and transport closure PASS')
