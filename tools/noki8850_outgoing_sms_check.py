#!/usr/bin/env python3
"""Verify the NSM-2 physical outgoing A/5551234 SMS transaction."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.radio_call_lifecycle_common import require_ordered
from PIL import Image

SUBMIT = '390118000100069121436587090d11010781551532f40000a70141'


def verify(text: str, product: str = '8850', key_separator: str = '',
           allow_message_reference: bool = False, rejected: bool = False) -> None:
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
        ('network RP result', re.compile(
            r'GSM service downlink kind=19 sapi=3 pd=09 message=01 length=7'
            if rejected else r'GSM service downlink kind=18 sapi=3 pd=09 message=01 length=5')),
        ('handset CP-ACK', re.compile(r'GSM service uplink sapi=3 pd=09 message=04 length=2 data=3904')),
        ('RR release', re.compile(r'LAPDm service Channel Release acknowledged')),
        ('return to paging', re.compile(r'PCH no-identity fill')),
    ), product + ' outgoing SMS')
    if text.count('gsm_sms_submit:') != 1:
        raise ValueError('expected one accepted SMS submission')
    if rejected and 'GSM service downlink kind=18 sapi=3' in text:
        raise ValueError('success RP-ACK appeared in rejected transaction')


def verify_silence(text: str, product: str = '8850') -> None:
    require_ordered(text, (
        ('physical Send', re.compile(re.escape(product) + r'_sms_send_physical: action=confirm_send')),
        ('exact submission', re.compile(r'GSM service uplink sapi=3 pd=09 message=01 length=27 data=' + SUBMIT + r'\b')),
        ('network CP-ACK', re.compile(r'GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2')),
        ('host silence accepted', re.compile(r'gsm_call_adapter: sms decision id=1 outcome=3 result=accepted')),
        ('mobile main-link DISC', re.compile(r'TX packet type=1b .*data=0080015301')),
        ('network UA', re.compile(r'RX enqueue type=80 .*data=80[0-9a-f]{18}017301')),
        ('physical deconfiguration', re.compile(r'TX packet type=02 .*radio_phase=release_channel_change')),
        ('correlated host end', re.compile(r'gsm_call_adapter: sms state id=1 epoch=1 phase=ended')),
        ('resumed paging', re.compile(r'PCH no-identity fill')),
    ), product + ' RP silence')
    if text.count('gsm_sms_submit:') != 1:
        raise ValueError('expected one silent SMS submission')
    if re.search(r'GSM service downlink kind=(?:18|19) sapi=3', text):
        raise ValueError('RP result appeared in silent transaction')


def check_recovery(text: str, frames: Path, *, rp_silence: bool = False,
                   product: str = '8850', key_separator: str = '') -> None:
    require_ordered(text, (
        ('release', re.compile(r'TX packet type=02 .*radio_phase=release_channel_change'
                              if rp_silence else r'LAPDm service Channel Release acknowledged')),
        ('physical End', re.compile(re.escape(product) + r'_sms_recovery_physical: key=End')),
        ('End decode', re.compile(re.escape(product + '_keypad_decoded' + key_separator) + r' key=0f\b')),
        ('physical Menu', re.compile(re.escape(product) + r'_sms_recovery_physical: key=Menu')),
        ('Menu decode', re.compile(re.escape(product + '_keypad_decoded' + key_separator) + r' key=19\b')),
    ), product + ' failed SMS recovery')

    def digest(path, crop):
        with Image.open(path) as source:
            if source.size != (84, 48):
                raise ValueError('unexpected handset frame geometry')
            return hashlib.sha256(source.convert('L').crop(crop).tobytes()).hexdigest()

    # Exclude the result icon; rejection and timeout have distinct text.
    failure_hash = ('cfcf7ce3d4d46e96949742863df35d03a4ea62c021e3e8e19493ec83be92f2a9'
                    if rp_silence else
                    '24aea298f2e0de336ee3fb10a5c767f912a07ec6eb14ca190e426b5c6320226a')
    if not any(digest(path, (0, 0, 60, 48)) == failure_hash
               for path in frames.glob(product + '_sms_reject_*.png')):
        raise ValueError('missing reviewed message-not-sent presentation')
    if digest(frames / (product + '_sms_recovery_menu.png'), (0, 0, 72, 16)) != (
            '1e5c11fcea9aac5331e18c0070697e8795003d6de9a0250284b3d9605209e654'):
        raise ValueError('missing reviewed Messages recovery menu')


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--rejected', action='store_true')
    parser.add_argument('--rp-silence', action='store_true')
    parser.add_argument('--recovery-frames', type=Path)
    args = parser.parse_args()
    try:
        text = args.log.read_text(errors='replace')
        if args.rp_silence:
            if args.rejected:
                raise ValueError('RP silence and rejection are distinct outcomes')
            verify_silence(text)
        else:
            verify(text, rejected=args.rejected)
        if args.recovery_frames:
            if not (args.rejected or args.rp_silence):
                raise ValueError('recovery frames require a failed transaction')
            check_recovery(text, args.recovery_frames, rp_silence=args.rp_silence)
    except (OSError, ValueError) as error:
        print(f'FAIL - {error}', file=sys.stderr)
        return 1
    print('8850 outgoing SMS PASS: physical input, exact A/5551234, ' +
          ('RP silence' if args.rp_silence else 'RP rejection' if args.rejected else 'RP acceptance') + ', closure and paging')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
