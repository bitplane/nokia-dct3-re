"""Validate bounded software notification observations, not audio completion."""
import argparse
from collections import Counter
import json
import math
from pathlib import Path
import sys

SITES = {'publish': (0x292c1,0x56f8), 'callback': (0x20981,0x76f8),
         'vector': (0x22064,0x4a1e), 'wrapper': (0x20742,0xf495),
         'request': (0x20780,0x7102), 'input16': (0x207fa,0x76f8),
         'input17': (0x207f5,0x76f8), 'receive16': (0x29458,0xe781)}


def records(text, marker):
    result = []
    for line in text.splitlines():
        if marker + ':' not in line:
            continue
        fields = {}
        for token in line.split(marker + ':', 1)[1].split():
            key, separator, value = token.partition('=')
            if not separator or key in fields:
                raise ValueError(f'malformed {marker}')
            fields[key] = value
        result.append(fields)
    return result


def check(text):
    def single(marker):
        values = records(text, marker)
        if len(values) != 1:
            raise ValueError(f'missing or duplicate {marker}')
        return values[0]

    scope, end, stop, parent = [single(name) for name in (
        'mu4_notify_scope', 'mu4_notify_terminal', 'mu4_notify_stop', 'mu4_playback_parent_terminal')]
    events = records(text, 'mu4_notify')
    if scope != dict(identity='Execute_entry', operands='context_only', observer_writes='0', audio_acceptance='0'):
        raise ValueError('notification scope changed')
    try:
        if int(end['cap']) != 4096 or not 0 < len(events) == int(end['events']) < 4096:
            raise ValueError('missing or capped notification events')
        when = float(stop['t'])
        if not math.isfinite(when) or when < 10 or stop['illegal'] != '0':
            raise ValueError('short or faulted notification observation')
        for key, value in dict(observer_writes='0', audio_acceptance='0').items():
            if end[key] != value:
                raise ValueError('terminal scope changed')
        for key, value in dict(first_session_complete='1', overlay_verified='1', illegal='0',
                               opcode_mismatch='0', selection='first_verified_aa99_session',
                               identity='Execute_entry', observer_writes='0', audio_acceptance='0').items():
            if parent[key] != value:
                raise ValueError('unverified parent session')
        overlay = 'mu4_native_playback_overlay: PASS marker=aa99 records=3127 words=49843 checked_ram_words=49839 excluded_register_words=4 before_execution=1'
        residents = [line for line in text.splitlines() if 'mu4_playback_execute_entry: address=026d62' in line]
        if (text.count(overlay) != 1 or len(residents) < 2 or
                not text.index(residents[0]) < text.index('mu4_notify:') or
                not text.index(overlay) < text.index('mu4_notify:') < text.rindex('mu4_notify:') < text.index(residents[1])):
            raise ValueError('notification outside verified first session')
        counts, requests, chain, previous, groups = Counter(), Counter(), [], -1, 0
        for index, event in enumerate(events, 1):
            kind, cycles = event['kind'], int(event['cycles'])
            if (int(event['ordinal']) != index or cycles <= previous or
                    SITES.get(kind) != (int(event['address'],16), int(event['opcode'],16))):
                raise ValueError('notification identity/opcode/order mismatch')
            previous = cycles
            for field in ('sp','f2','f3','f5','mailbox','flag','cbh','cbl'):
                if not 0 <= int(event[field],16) <= 0xffff:
                    raise ValueError('notification word outside domain')
            counts[kind] += 1
            if kind not in ('publish','callback','vector','wrapper','request'):
                continue
            chain.append(event)
            if len(chain) == 5:
                if [item['kind'] for item in chain] != ['publish','callback','vector','wrapper','request']:
                    raise ValueError('incomplete notification chain')
                if (len({item['mailbox'] for item in chain}) != 1 or
                        any((item['cbh'],item['cbl']) != ('0002','0981') for item in chain) or
                        [item['flag'] for item in chain] != ['0000','0000','0001','0001','0000'] or
                        chain[-1]['f2'] != chain[-1]['mailbox']):
                    raise ValueError('notification context correlation mismatch')
                requests[chain[-1]['f2']] += 1
                groups += 1
                chain = []
        if chain or not groups:
            raise ValueError('censored notification chain')
    except (KeyError, TypeError, OverflowError) as error:
        raise ValueError('incomplete notification fields') from error
    return dict(events=len(events), groups=groups, counts=dict(counts), requests=dict(requests),
                selected_controls={kind: counts[kind] for kind in ('input16','input17','receive16')},
                scope='Executed notification entries with context correlation, not retained operands, '
                      'retirement/annulment proof, physical latency, all producers, EOF policy or audio acceptance')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    try:
        result = check(args.log.read_text())
    except (ValueError, OSError) as error:
        parser.exit(1, f'Notification check: {error}\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
