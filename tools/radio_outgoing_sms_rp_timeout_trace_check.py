#!/usr/bin/env python3
"""Verify NSE-8 physical SMS submission and firmware-owned RP timeout."""
import hashlib
import argparse
from pathlib import Path
import re

try:
    from tools.radio_outgoing_sms_trace_check import CHECKPOINTS
    from tools.radio_outgoing_sms_timeout_trace_check import MESSAGE_SENDING_FAILED_HASH
    from tools.radio_call_lifecycle_common import require_ordered
    from tools.radio_state_roundtrip import verify_roundtrip, ROUNDTRIP_RE, MARKER_RE
    from tools.radio_speech_media_trace_check import canonical_timeline
except ModuleNotFoundError:
    from radio_outgoing_sms_trace_check import CHECKPOINTS
    from radio_outgoing_sms_timeout_trace_check import MESSAGE_SENDING_FAILED_HASH
    from radio_call_lifecycle_common import require_ordered
    from radio_state_roundtrip import verify_roundtrip, ROUNDTRIP_RE, MARKER_RE
    from radio_speech_media_trace_check import canonical_timeline


def verify(text, frames=None):
    require_ordered(text, CHECKPOINTS[:6], '3210 RP-silence submission')
    submits = list(re.finditer(r'gsm_sms_submit: .*outcome=3 status_report=0 t=([0-9.]+)', text))
    if len(submits) != 1 or text.count('gsm_sms_submit:') != 1:
        raise ValueError('expected exactly one RP-silence submission')
    if re.search(r'GSM service downlink kind=(?:18|19) sapi=3', text):
        raise ValueError('network supplied an RP result during silence')
    cp_ack = CHECKPOINTS[5][1]
    if len(cp_ack.findall(text)) != 1:
        raise ValueError('expected exactly one network CP-ACK')
    tail = text[submits[0].end():]
    disc = re.search(r'TX pending type=1b .*data=0080015301[0-9a-f]* t=([0-9.]+)', tail)
    if not disc or not 60 <= float(disc[1]) - float(submits[0][1]) <= 80:
        raise ValueError('firmware main-link DISC absent or outside observed RP-timeout window')
    require_ordered(tail, (
        ('CP-ACK', cp_ack),
        ('mobile main-link DISC', re.compile(r'TX pending type=1b .*data=0080015301')),
        ('network UA', re.compile(r'RX enqueue type=80 .*data=80[0-9a-f]{18}017301')),
        ('channel deconfiguration', re.compile(
            r'TX packet type=02 .*radio_phase=release_channel_change '
            r'data=041202000000001a600000010000000f00000000')),
        ('resumed paging', re.compile(r'PCH no-identity fill')),
    ), '3210 RP-timeout clearing')
    if frames is not None and not any(
            hashlib.sha256(path.read_bytes()).hexdigest() == MESSAGE_SENDING_FAILED_HASH
            for path in Path(frames).glob('nokia_dct3_lcdmirror_*.pgm')):
        raise ValueError('reviewed Message sending failed pixels absent')


def verify_state(text, frames=None):
    verify_roundtrip(text, (
        'dsp_hle: TX packet type=', 'dspif_transport: RX enqueue type=',
        'dsp_hle: LAPDm ', 'dsp_hle: GSM service ',
    ), '3210 RP-result wait')
    timeline = canonical_timeline(text)
    verify(timeline, frames)
    saved = float(ROUNDTRIP_RE.search(text)[2])
    cp_ack = re.search(r'GSM service downlink kind=17 sapi=3 .* t=([0-9.]+)', timeline)
    restored = {match[2]: float(match[3]) for match in MARKER_RE.finditer(text)
                if match[1] == 'restored'}
    disc = re.search(r'TX pending type=1b .*data=0080015301[0-9a-f]* t=([0-9.]+)', timeline)
    if not cp_ack or not float(cp_ack[1]) < saved < float(disc[1]):
        raise ValueError('save did not occur after CP-ACK and before RP-timeout clearing')
    if not restored['begin'] < float(disc[1]) < restored['end']:
        raise ValueError('deterministic restored interval did not include timeout clearing')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('frames', type=Path)
    parser.add_argument('--require-state', action='store_true')
    args = parser.parse_args()
    try:
        (verify_state if args.require_state else verify)(args.log.read_text(errors='replace'), args.frames)
    except (OSError, ValueError) as error:
        raise SystemExit(f'FAIL: {error}') from None
    print('OK - physical 3210 RP timeout, failure pixels and resumed paging' +
          ('; deterministic RP-wait save/load replay' if args.require_state else ''))


if __name__ == '__main__':
    main()
