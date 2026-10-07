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
from tools.noki8850_outgoing_sms_check import SUBMIT
from tools.noki8890_registration_check import verify as verify_registration
from tools.radio_call_lifecycle_common import require_ordered


def verify(text, *, pcs1900=False, rejected=False):
    if pcs1900:
        verify_registration(text, pcs1900=True)
    verify_submission(text, product='8890', key_separator=':', rejected=rejected)


def check_recovery(text, frames, *, rp_silence=False):
    if not re.search(r'(?:LAPDm service Channel Release acknowledged|'
                     r'TX packet type=02 .*radio_phase=release_channel_change)[\s\S]*'
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
    # Exclude the animated result icon; rejection and timeout have different text.
    failure_hash = ('69de12aaacc90d5c255b54b29b864b51bb37212bd98e307a6c2d64a9c4f89113'
                    if rp_silence else
                    '5838beb8b9ec96c1fb92a4cf39a6f5707bddd2f2e44ac68f7039c811a511bc83')
    if not any(digest(path, (0, 0, 60, 48)) == failure_hash
               for path in frames.glob('8890_sms_reject_*.png')):
        raise ValueError('missing reviewed message-not-sent/timeout presentation')
    if digest(frames / '8890_sms_recovery_menu.png', (0, 0, 72, 16)) != (
            'da31a6b8a573a7211b4eb55ffd4a8c05795988230cc5d3acfdf190fea65fe4c0'):
        raise ValueError('missing reviewed recovery menu')


def verify_silence(text, *, pcs1900=False):
    if pcs1900:
        verify_registration(text, pcs1900=True)
    require_ordered(text, (
        ('physical Send', re.compile(r'8890_sms_send_physical: action=confirm_send')),
        ('exact submission', re.compile(r'GSM service uplink sapi=3 pd=09 message=01 length=27 data=' + SUBMIT)),
        ('network CP-ACK', re.compile(r'GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2')),
        ('host silence accepted', re.compile(r'gsm_call_adapter: sms decision id=1 outcome=3 result=accepted')),
        ('mobile main-link DISC', re.compile(r'TX packet type=1b .*data=0080015301')),
        ('network UA', re.compile(r'RX enqueue type=80 .*data=80[0-9a-f]{18}017301')),
        ('physical deconfiguration', re.compile(r'TX packet type=02 .*radio_phase=release_channel_change')),
        ('correlated host end', re.compile(r'gsm_call_adapter: sms state id=1 epoch=1 phase=ended')),
        ('resumed paging', re.compile(r'PCH no-identity fill')),
    ), '8890 RP silence')
    if text.count('gsm_sms_submit:') != 1:
        raise ValueError('expected one silent SMS submission')
    if re.search(r'GSM service downlink kind=(?:18|19) sapi=3', text):
        raise ValueError('RP result appeared in silent transaction')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--pcs1900', action='store_true')
    parser.add_argument('--rejected', action='store_true')
    parser.add_argument('--rp-silence', action='store_true')
    parser.add_argument('--recovery-frames', type=Path)
    args = parser.parse_args()
    try:
        text = args.log.read_text(errors='replace')
        if args.rp_silence:
            if args.rejected:
                raise ValueError('RP silence and rejection are distinct outcomes')
            verify_silence(text, pcs1900=args.pcs1900)
        else:
            verify(text, pcs1900=args.pcs1900, rejected=args.rejected)
        if args.recovery_frames:
            if not (args.rejected or args.rp_silence):
                raise ValueError('recovery frames require failed transaction')
            check_recovery(text, args.recovery_frames, rp_silence=args.rp_silence)
    except (OSError, ValueError) as error:
        print(f'FAIL - {error}', file=sys.stderr)
        return 1
    print('8890 outgoing SMS PASS: physical A/5551234, ' +
          ('RP silence' if args.rp_silence else 'RP rejection' if args.rejected else 'RP acceptance') + ', closure and paging')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
