"""Check bounded debugger Execute-hook cleanup paths; operands remain context."""
import argparse
from collections import Counter
import json
from pathlib import Path
import sys

if __package__:
    from .mu4_playback_frame_check import rows
else:
    from mu4_playback_frame_check import rows

SITES = {0x29438: 0x1004, 0x2944b: 0x7105, 0x29471: 0x1005,
         0x294c3: 0xf84a, 0x294ca: 0xf847, 0x294e6: 0x6c82,
         0x294fd: 0x60f8, 0x29500: 0xf830, 0x29502: 0xf7b8, 0x2956f: 0x60f8}


def check(text):
    events = rows(text, 'mu4_cleanup_ingress')
    scopes = rows(text, 'mu4_cleanup_ingress_scope')
    ends = rows(text, 'mu4_cleanup_ingress_terminal')
    stops = rows(text, 'mu4_cleanup_ingress_stop')
    parents = rows(text, 'mu4_playback_parent_terminal')
    if (len(scopes) != 1 or scopes[0] != {'identity': 'debugger_Execute_hook',
            'operands': 'context_only', 'annulment': 'unclaimed',
            'observer_writes': '0', 'audio_acceptance': '0'}
            or len(ends) != 1 or len(stops) != 1 or len(parents) != 1):
        raise ValueError('missing or changed ingress scope/terminal')
    end, stop, parent = ends[0], stops[0], parents[0]
    if (not 0 < len(events) == int(end['events']) < int(end['cap'])
            or end['audio_acceptance'] != '0' or stop['audio_acceptance'] != '0'
            or stop['illegal'] != '0' or float(stop['t']) < 9.86
            or any(parent.get(key) != value for key, value in {
                'first_session_complete': '1', 'overlay_verified': '1', 'illegal': '0',
                'opcode_mismatch': '0', 'selection': 'first_verified_aa99_session'}.items())):
        raise ValueError('incomplete or capped ingress session')
    overlay = 'mu4_native_playback_overlay: PASS marker=aa99 records=3127 words=49843 checked_ram_words=49839 excluded_register_words=4 before_execution=1'
    if text.count(overlay) != 1 or text.index(overlay) > text.index('mu4_cleanup_ingress:'):
        raise ValueError('missing original AA99 verification before ingress')
    residents = [line for line in text.splitlines() if 'mu4_playback_execute_entry: address=026d62' in line]
    if len(residents) < 2 or not (text.index(residents[0]) < text.index('mu4_cleanup_ingress:')
                                < text.rindex('mu4_cleanup_ingress:') < text.index(residents[1])):
        raise ValueError('ingress outside first resident session')
    previous = -1
    inputs, paths, length_paths = [], Counter(), Counter()
    cleanup = []
    for index, event in enumerate(events):
        address, cycles = int(event['address'], 16), int(event['cycles'])
        if (int(event['ordinal']) != index+1 or cycles <= previous
                or SITES.get(address) != int(event['opcode'], 16)
                or event.get('identity') != 'debugger_Execute_hook'
                or event.get('operands') != 'context_only'):
            raise ValueError('ingress identity/opcode/order mismatch')
        previous = cycles
        for field in ('sp', 'f4', 'f5', 'f6', 'f7', 'f8', 'f9', 'control', 'ar1', 'ar2'):
            if not 0 <= int(event[field], 16) <= 0xffff:
                raise ValueError('ingress word outside domain')
        if address == 0x29438:
            inputs.append({key: event[key] for key in ('cycles', 'f4', 'f5', 'f6', 'f7', 'f8', 'f9')})
        if address == 0x294c3:
            if index+1 == len(events):
                raise ValueError('censored length branch')
            target = int(events[index+1]['address'], 16)
            if target not in (0x294ca, 0x294fd):
                raise ValueError('unresolved length successor')
            length_paths[f'{target:06x}'] += 1
        if address == 0x29500:
            if index+1 == len(events):
                raise ValueError('censored cleanup branch')
            target = int(events[index+1]['address'], 16)
            if target not in (0x29502, 0x2956f):
                raise ValueError('unresolved cleanup successor')
            paths[f'{target:06x}'] += 1
            if target == 0x2956f:
                cleanup.append({'cycles': cycles, 'control_context': event['control'],
                                'f7_context': event['f7']})
    if not cleanup:
        raise ValueError('no observed cleanup successor')
    return {'events': len(events), 'input_counts': dict(Counter(row['f4'] for row in inputs)),
            'last_inputs': inputs[-3:], 'cleanup_successors': dict(paths),
            'length_successors': dict(length_paths),
            'cleanup': cleanup, 'scope': 'Debugger hook at staged Execute entry; '
            'live snapshots/annulment are unclaimed, no retained operand, EOF, '
            'message producer ownership or audio acceptance proof'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(check(args.log.read_text()), indent=2))
    except (ValueError, KeyError, OSError) as error:
        print(f'cleanup ingress check: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
