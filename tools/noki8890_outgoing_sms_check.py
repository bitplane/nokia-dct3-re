#!/usr/bin/env python3
"""Verify NSB-6 physical A/5551234 SMS submission and transport closure."""
import argparse
from pathlib import Path
import sys
import hashlib
import re
from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.noki8850_outgoing_sms_check import verify as verify_submission
from tools.noki8890_registration_check import verify as verify_registration


def verify(text, *, pcs1900=False, rejected=False):
    if pcs1900:
        verify_registration(text, pcs1900=True)
    verify_submission(text, product='8890', key_separator=':', rejected=rejected)


def check_recovery(text, frames):
    if not re.search(r'LAPDm service Channel Release acknowledged[\s\S]*'
                     r'8890_sms_recovery_physical: key=End[\s\S]*'
                     r'8890_keypad_decoded: key=0f\b[\s\S]*'
                     r'8890_sms_recovery_physical: key=Menu[\s\S]*'
                     r'8890_keypad_decoded: key=19\b', text):
        raise ValueError('missing post-rejection physical recovery')
    def digest(path, crop):
        with Image.open(path) as source:
            if source.size != (84, 48):
                raise ValueError('unexpected handset frame geometry')
            return hashlib.sha256(source.convert('L').crop(crop).tobytes()).hexdigest()
    # Exclude the animated result icon; retain all three lines of failure text.
    if not any(digest(path, (0, 0, 60, 48)) ==
               '5838beb8b9ec96c1fb92a4cf39a6f5707bddd2f2e44ac68f7039c811a511bc83'
               for path in frames.glob('8890_sms_reject_*.png')):
        raise ValueError('missing reviewed message-not-sent presentation')
    if digest(frames / '8890_sms_recovery_menu.png', (0, 0, 72, 16)) != (
            'da31a6b8a573a7211b4eb55ffd4a8c05795988230cc5d3acfdf190fea65fe4c0'):
        raise ValueError('missing reviewed recovery menu')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--pcs1900', action='store_true')
    parser.add_argument('--rejected', action='store_true')
    parser.add_argument('--recovery-frames', type=Path)
    args = parser.parse_args()
    try:
        text = args.log.read_text(errors='replace')
        verify(text, pcs1900=args.pcs1900, rejected=args.rejected)
        if args.recovery_frames:
            if not args.rejected:
                raise ValueError('recovery frames require rejected transaction')
            check_recovery(text, args.recovery_frames)
    except (OSError, ValueError) as error:
        print(f'FAIL - {error}', file=sys.stderr)
        return 1
    print('8890 outgoing SMS PASS: physical A/5551234, ' +
          ('RP rejection' if args.rejected else 'RP acceptance') + ', closure and paging')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
