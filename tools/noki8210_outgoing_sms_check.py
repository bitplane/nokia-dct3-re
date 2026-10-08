"""Verify physical NSM-3 A/5551234 submission using shared GSM checks."""
import argparse
import hashlib
from pathlib import Path
import sys
from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.noki8850_outgoing_sms_check import verify as verify_submission
from tools.noki8850_outgoing_sms_check import verify_silence, check_recovery


def verify(text, *, rejected=False):
    if '[LUA ERROR]' in text:
        raise ValueError('fixture error')
    verify_submission(text, product='8210', key_separator=':', allow_message_reference=True,
                      rejected=rejected)


def check_sent_frame(path):
    with Image.open(path) as frame:
        # The envelope on the right is animated; retain the full success text.
        if frame.size != (84, 48) or hashlib.sha256(
                frame.convert('L').crop((0, 0, 64, 48)).tobytes()).hexdigest() != (
                'b091782f84539a43810cc3ffd1e81697b21140078471cc2b3ea2fbda6a338ca3'):
            raise ValueError('missing reviewed Message sent text')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    outcome = parser.add_mutually_exclusive_group()
    outcome.add_argument('--rejected', action='store_true')
    outcome.add_argument('--rp-silence', action='store_true')
    parser.add_argument('--recovery-frames', type=Path)
    parser.add_argument('--sent-frame', type=Path)
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
        if args.sent_frame:
            if args.rejected or args.rp_silence:
                raise ValueError('success frame cannot validate a failed SMS')
            check_sent_frame(args.sent_frame)
    except (OSError, ValueError) as error:
        parser.exit(1, f'8210 outgoing SMS FAIL: {error}\n')
    print('8210 physical A/5551234 ' +
          ('RP silence' if args.rp_silence else 'RP rejection' if args.rejected else 'SMS submission') +
          ' and transport closure PASS')
