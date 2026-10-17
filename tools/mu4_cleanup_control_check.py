"""Check passive cleanup control bus observations; no message ownership claim."""
import argparse
import json
from pathlib import Path
import sys

if __package__:
    from .mu4_playback_frame_check import rows
else:
    from mu4_playback_frame_check import rows


def check(text):
    scopes = rows(text, 'mu4_cleanup_control_scope')
    ends = rows(text, 'mu4_cleanup_control_terminal')
    if len(scopes) != 1 or scopes[0] != {
            'address': 'aa1a', 'boundary': 'data_bus', 'context_pc': 'not_owner',
            'observer_writes': '0', 'audio_acceptance': '0'} or len(ends) != 1:
        raise ValueError('missing or changed observation scope')
    end = ends[0]
    if any(end.get(key) != value for key, value in
           {'capped': '0', 'illegal': '0', 'audio_acceptance': '0'}.items()):
        raise ValueError('capped or invalid control observation')
    events = []
    for kind, count in (('write', 'writes'), ('read', 'reads')):
        selected = rows(text, 'mu4_cleanup_control_'+kind)
        if not 0 < len(selected) <= 32 or len(selected) != int(end[count]):
            raise ValueError('incomplete control event coverage')
        previous = -1.0
        for index, event in enumerate(selected, 1):
            when, value = float(event['t']), int(event['value'], 16)
            if (int(event['ordinal']) != index or not previous <= when <= float(end['t'])
                    or not 0 <= value <= 0xffff or not 0 <= int(event['context_pc'], 16) <= 0x7fffff
                    or (kind == 'write' and event.get('mask') != 'ffff')):
                raise ValueError('control event order, mask or domain changed')
            previous = when
            events.append((when, kind, value, event['context_pc']))
    # Equal-time read/write ordering cannot be recovered from independent lists.
    if len({event[0] for event in events}) != len(events):
        raise ValueError('ambiguous control event timestamps')
    retained = None
    observations = []
    for when, kind, value, pc in sorted(events):
        if kind == 'write':
            retained = (value, when)
        else:
            if retained is None or retained[0] != value:
                raise ValueError('read has no matching preceding full-word write')
            observations.append({'time': when, 'value': f'{value:04x}',
                                 'preceding_write_time': retained[1], 'context_pc': pc})
    return {'writes': int(end['writes']), 'reads': int(end['reads']),
            'read_observations': observations, 'terminal_time': float(end['t']),
            'scope': 'Full-word data-bus chronology within capture; context PCs are not '
                     'instruction/message ownership; no EOF or audio acceptance claim'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(check(args.log.read_text()), indent=2))
    except (ValueError, KeyError, OSError) as error:
        print(f'cleanup control check: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
