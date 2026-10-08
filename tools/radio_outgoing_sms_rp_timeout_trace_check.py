#!/usr/bin/env python3
"""Verify NSE-8 physical SMS submission and firmware-owned RP timeout."""
import hashlib
from pathlib import Path
import re
import sys

try:
    from tools.radio_outgoing_sms_trace_check import CHECKPOINTS
    from tools.radio_outgoing_sms_timeout_trace_check import MESSAGE_SENDING_FAILED_HASH
    from tools.radio_call_lifecycle_common import require_ordered
except ModuleNotFoundError:
    from radio_outgoing_sms_trace_check import CHECKPOINTS
    from radio_outgoing_sms_timeout_trace_check import MESSAGE_SENDING_FAILED_HASH
    from radio_call_lifecycle_common import require_ordered


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


def main():
    if len(sys.argv) != 3:
        raise SystemExit('usage: radio_outgoing_sms_rp_timeout_trace_check.py LOG FRAME_DIR')
    try:
        verify(Path(sys.argv[1]).read_text(errors='replace'), Path(sys.argv[2]))
    except (OSError, ValueError) as error:
        raise SystemExit(f'FAIL: {error}') from None
    print('OK - physical 3210 RP timeout, failure pixels and resumed paging')


if __name__ == '__main__':
    main()
