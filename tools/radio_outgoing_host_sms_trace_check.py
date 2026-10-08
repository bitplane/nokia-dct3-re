#!/usr/bin/env python3
"""Check the host-decided outgoing SMS lifecycle."""

import pathlib
import re
import argparse


CHECKPOINTS = (
    re.compile(r"gsm_call_adapter: sms request id=1 epoch=1 recipient=5551234 alphabet=gsm7 octets=2"),
    re.compile(r"gsm_call_adapter: sms decision id=2 outcome=1 result=rejected"),
    re.compile(r"gsm_call_adapter: sms decision id=1 outcome=0 result=accepted"),
    re.compile(r"gsm_call_adapter: sms decision id=1 outcome=0 result=rejected"),
    re.compile(r"GSM service downlink kind=18 sapi=3 pd=09 message=01 length=5"),
    re.compile(r"gsm_call_adapter:.*sms.*phase=ended|GSM service uplink sapi=3 pd=09 message=04 length=2 data=2904"),
)


def verify(text: str, *, octets: int = 2, outcome: str = 'rp_ack') -> None:
    if not 0 <= octets <= 140:
        raise ValueError('SMS octet count outside TP-UD range')
    checkpoints = list(CHECKPOINTS)
    outcomes = {'rp_ack': 0, 'rp_error': 1, 'rp_silence': 3}
    if outcome not in outcomes:
        raise ValueError('unknown host SMS outcome')
    code = outcomes[outcome]
    checkpoints[2] = re.compile(rf'gsm_call_adapter: sms decision id=1 outcome={code} result=accepted')
    checkpoints[3] = re.compile(rf'gsm_call_adapter: sms decision id=1 outcome={code} result=rejected')
    if outcome == 'rp_error':
        checkpoints[4] = re.compile(r'GSM service downlink kind=19 sapi=3 pd=09 message=01 length=7')
    elif outcome == 'rp_silence':
        checkpoints[4:] = [
            re.compile(r'TX packet type=1b .*data=0080015301'),
            re.compile(r'RX enqueue type=80 .*data=80[0-9a-f]{18}017301'),
            re.compile(r'TX packet type=02 .*radio_phase=release_channel_change'),
            re.compile(r'gsm_call_adapter: sms state id=1 epoch=1 phase=ended'),
        ]
    checkpoints[0] = re.compile(
        rf"gsm_call_adapter: sms request id=1 epoch=1 recipient=5551234 alphabet=gsm7 octets={octets}\b")
    cursor = 0
    for pattern in checkpoints:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(f"missing host-SMS checkpoint: {pattern.pattern}")
        cursor = match.end()
    if outcome != 'rp_ack' and re.search(r'GSM service downlink kind=18 sapi=3', text):
        raise ValueError('success RP-ACK appeared during failed host SMS')
    if outcome == 'rp_silence' and re.search(r'GSM service downlink kind=19 sapi=3', text):
        raise ValueError('RP-ERROR appeared during host silence')


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=pathlib.Path)
    parser.add_argument('--octets', type=int, default=2)
    parser.add_argument('--outcome', choices=('rp_ack', 'rp_error', 'rp_silence'), default='rp_ack')
    args = parser.parse_args()
    try:
        verify(args.log.read_text(), octets=args.octets, outcome=args.outcome)
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print("OK - host SMS request, decision and firmware CP/RP lifecycle completed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
