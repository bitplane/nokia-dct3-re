#!/usr/bin/env python3
"""Require transport and own-ROM delivery evidence for an NHM-3 8b probe."""
import argparse
from pathlib import Path
import re
import sys


def verify(text):
    start = re.search(
        r'RX enqueue type=8b payload=166 producer=([0-9a-f]+).*?t=([0-9.]+)', text)
    if start is None:
        raise ValueError('missing 166-byte 8b publication')
    producer, published = int(start[1], 16), float(start[2])
    events = text[start.end():]
    # This instrument checks one isolated candidate, not a batched stream.
    # A later measurement can reuse ring positions and heap envelopes.
    events = re.split(r'RX enqueue type=8b\b', events, maxsplit=1)[0]
    notified = re.search(
        rf'FIQ0 notify producer={producer:03x} consumer=[0-9a-f]+ t=([0-9.]+)', events)
    if notified is None or float(notified[1]) < published:
        raise ValueError('8b published without a matching RX notification')
    events = events[notified.end():]
    consumed = re.search(
        rf'RAM W off=1ca data={producer:04x} t=([0-9.]+)', events)
    if consumed is None:
        raise ValueError('notified 8b publication lacks consumer advance to its end')
    route = re.search(
        r'6250_pin_rssi_route: enable=01 message=([0-9a-f]+) t=([0-9.]+)', events)
    if route is None:
        raise ValueError('missing enabled own-ROM 8b handler entry')
    # Timestamp order, rather than log order, permits instruction-fetch taps
    # and transport writes to be recorded at the same emulated instant.
    routed = float(route[2])
    if routed < float(notified[1]):
        raise ValueError('8b handler entry precedes this notification')
    posted = re.search(
        rf'6250_pin_radio_post: target=0e class=8b caller=004649c7 '
        rf'message={route[1]} t=([0-9.]+)', events)
    if posted is None or float(posted[1]) < routed:
        raise ValueError('8b handler did not forward its envelope to task 14')
    return {'producer': producer, 'published': published, 'routed': routed,
            'message': int(route[1], 16)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    try:
        result = verify(args.log.read_text(errors='replace'))
    except (OSError, ValueError) as error:
        print(f'6250 measurement delivery: FAIL: {error}', file=sys.stderr)
        return 1
    print(f'6250 measurement delivery: PASS: {result}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
