"""Validate bounded decoder branch observations, not EOF or audio acceptance."""
import argparse
from collections import Counter
import json
from pathlib import Path
import re
import sys

if __package__:
    from .mu4_playback_frame_check import rows
else:
    from mu4_playback_frame_check import rows

SITES = dict(zip(
    (0x2936d, 0x293a2, 0x293a8, 0x293aa, 0x293ad, 0x293ae,
     0x293b0, 0x293b5, 0x293b7, 0x293cd, 0x293cf, 0x2940b),
    (0x10f8, 0x10f8, 0xf982, 0x8812, 0xf5aa, 0xf830,
     0x7211, 0xf845, 0x10f8, 0xf982, 0x8811, 0x10f8)))


def check(text):
    events = rows(text, 'mu4_decoder_gate')
    terminals = rows(text, 'mu4_decoder_gate_terminal')
    parents = rows(text, 'mu4_playback_parent_terminal')
    if len(terminals) != 1 or len(parents) != 1:
        raise ValueError('missing or repeated terminal')
    end, parent = terminals[0], parents[0]
    first, last = float(end['first']), float(end['last'])
    if (not events or len(events) != int(end['events']) or len(events) > 512
            or end['bad'] != '0' or end['capped'] != '0'
            or float(end['t']) < last or first >= last):
        raise ValueError('incomplete gate window')
    if end.get('identity') != 'Execute_entry' or end.get('operands') != 'context_only':
        raise ValueError('gate terminal scope changed')
    residents = [float(value) for value in re.findall(
        r'mu4_playback_execute_entry: address=026d62 opcode=fd00 t=([0-9.]+)', text)]
    if len(residents) < 2 or not residents[0] < first < last < residents[1]:
        raise ValueError('window not bounded inside first resident session')
    if any(parent.get(key) != value for key, value in {
            'first_session_complete': '1', 'overlay_verified': '1', 'illegal': '0',
            'opcode_mismatch': '0', 'selection': 'first_verified_aa99_session',
            'identity': 'Execute_entry', 'audio_acceptance': '0'}.items()) or float(parent['t']) < last:
        raise ValueError('unverified parent session')
    branches = Counter()
    previous = first
    for ordinal, event in enumerate(events, 1):
        address = int(event['address'], 16)
        when = float(event['t'])
        if (int(event['ordinal']) != ordinal or not previous <= when <= last
                or event.get('identity') != 'Execute_entry'
                or event.get('operands') != 'context_only'
                or SITES.get(address) != int(event['opcode'], 16)):
            raise ValueError('gate identity, opcode or chronology mismatch')
        previous = when
        if address in (0x293ae, 0x293b5):
            if ordinal == len(events):
                raise ValueError('censored branch outcome')
            following = events[ordinal]
            target = int(following['address'], 16)
            passed = 0x293b0 if address == 0x293ae else 0x293b7
            if target not in (passed, 0x2940b) or following['call'] != event['call']:
                raise ValueError('unresolved branch successor')
            branches[(event['call'], 'availability' if address == 0x293ae else 'activity',
                      'pass' if target == passed else 'skip')] += 1
    return {'events': len(events), 'window': [first, last],
            'last_event_time': float(events[-1]['t']),
            'branches': [{'processing_count': int(call), 'gate': gate, 'outcome': outcome, 'count': count}
                         for (call, gate, outcome), count in sorted(branches.items())],
            'scope': 'Executed successors in bounded window; operands are context only; '
                     'processing_count is not a virtual-method attempt ID; '
                     'no EOF, physical units or audio acceptance claim'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(check(args.log.read_text()), indent=2))
    except (ValueError, KeyError, OSError) as error:
        print(f'decoder gate check: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
