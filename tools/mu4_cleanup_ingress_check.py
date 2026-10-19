"""Check bounded debugger Execute-hook cleanup paths; operands remain context."""
import argparse
from collections import Counter
import hashlib
import json
import math
import re
from pathlib import Path
import sys

if __package__:
    from .mu4_playback_frame_check import rows
else:
    from mu4_playback_frame_check import rows

SITES = {0x29438: 0x1004, 0x2944b: 0x7105, 0x29471: 0x1005,
         0x294c3: 0xf84a, 0x294ca: 0xf847, 0x294e6: 0x6c82,
         0x294fd: 0x60f8, 0x29500: 0xf830, 0x29502: 0xf7b8, 0x2956f: 0x60f8}


def reference_input_extent(byte_length, requests):
    """Source-model arithmetic for an even file, zero initial counter and positive requests."""
    if byte_length <= 0 or byte_length % 2 or byte_length > 0xffffffff:
        raise ValueError('input extent model requires a positive even 32-bit byte length')
    limit, consumed = byte_length // 2, 0
    records = []
    for index, request in enumerate(requests, 1):
        if records and records[-1]['crosses']:
            raise ValueError('requests continue after modeled cleanup boundary')
        if not 0 < request < 0x8000:
            raise ValueError('input extent model requires positive signed-word requests')
        crosses = consumed + request >= limit
        records.append({'input': index, 'prior_words': consumed, 'request_words': request,
                        'remaining_words': limit-consumed, 'crosses': crosses,
                        'expected_successor': '0294fd' if crosses else '0294ca'})
        consumed += request
    if not records or not records[-1]['crosses']:
        raise ValueError('input extent model never reaches the boundary')
    return records


def check(text, media=None, *, first_resident_only=False):
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
    if (not 0 < len(events) == int(end['events']) <= int(end['cap'])
            or (len(events) == int(end['cap']) and not first_resident_only)
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
    total_events = len(events)
    if first_resident_only:
        if len(residents) < 2:
            raise ValueError('missing first resident-session closing boundary')
        first, last = text.index(residents[0]), text.index(residents[1])
        events = rows(text[first:last], 'mu4_cleanup_ingress')
        if not events or int(events[-1]['ordinal']) >= int(end['cap']):
            raise ValueError('first resident session may have reached the capture cap')
        selected_last = first + text[first:last].rindex('mu4_cleanup_ingress:')
    else:
        selected_last = text.rindex('mu4_cleanup_ingress:')
    if len(residents) < 2 or not (text.index(residents[0]) < text.index('mu4_cleanup_ingress:')
                                < selected_last < text.index(residents[1])):
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
    result = {'events': len(events), 'input_counts': dict(Counter(row['f4'] for row in inputs)),
            'last_inputs': inputs[-3:], 'cleanup_successors': dict(paths),
            'length_successors': dict(length_paths),
            'cleanup': cleanup, 'scope': 'Debugger hook at staged Execute entry; '
            'live snapshots/annulment are unclaimed, no retained operand, EOF, '
            'message producer ownership or audio acceptance proof'}
    if first_resident_only:
        result['capture_selection'] = dict(scope='First closed resident session only',
            captured_events=total_events, excluded_later_events=total_events-len(events),
            global_cap_reached=total_events == int(end['cap']), selected_session_uncapped=True)
    if media is not None:
        if any(row['f4'] != '0011' for row in inputs):
            raise ValueError('extent model does not cover other input selectors')
        model = reference_input_extent(len(media), [int(row['f7'], 16) for row in inputs])
        observed = []
        for index, event in enumerate(events):
            if event['address'] == '0294c3':
                observed.append(events[index+1]['address'])
        if observed != [row['expected_successor'] for row in model]:
            raise ValueError('length successors disagree with the input extent model')
        result['input_extent_model'] = {
            'file_bytes': len(media), 'file_sha256': hashlib.sha256(media).hexdigest(),
            'limit_words': len(media)//2, 'requests': len(model), 'final': model[-1],
            'scope': 'Zero-initial-counter/even-file source model matches executed successors; '
                     'not an independent bus capture of every counter or file-read retirement'}
    return result


def check_lifecycle_restart(text):
    """Require ordered bus writes and resident re-entry, not EOF acceptance."""
    writes = [row for row in rows(text, 'mu4_playback_lifecycle_write')
              if row['address'] == '373e']
    terminals = rows(text, 'mu4_playback_lifecycle_terminal')
    # This legacy driver entry record predates observer_writes-tagged rows.
    residents = list(re.finditer(
        r'mu4_playback_execute_entry: address=026d62 opcode=fd00 t=([0-9.]+)\b', text))
    if len(terminals) != 1 or len(residents) < 2 or not writes:
        raise ValueError('missing lifecycle or resident restart evidence')
    terminal = terminals[0]
    if (terminal.get('illegal') != '0' or terminal.get('observer_writes') != '0'
            or terminal.get('audio_acceptance') != '0'
            or int(terminal['lifecycle_writes']) != len(writes)):
        raise ValueError('incomplete lifecycle write census')
    previous = -1.0
    for ordinal, row in enumerate(writes, 1):
        time = float(row['t'])
        if (int(row['ordinal']) != ordinal or not math.isfinite(time) or time <= previous
                or row.get('mask') != 'ffff' or row.get('observer_writes') != '0'):
            raise ValueError('invalid lifecycle bus chronology')
        previous = time
    starts = [index for index, row in enumerate(writes) if row['value'] == '0005']
    if len(starts) != 1 or starts[0] == 0:
        raise ValueError('expected one observed cleanup lifecycle')
    index = starts[0]
    sequence = writes[index-1:index+4]
    if [row['value'] for row in sequence] != ['0004', '0005', '0007', '0003', '0004']:
        raise ValueError('incomplete cleanup/restart sequence')
    resident_time = float(residents[1].group(1))
    if not float(sequence[1]['t']) < resident_time < float(sequence[2]['t']):
        raise ValueError('resident return does not separate cleanup and restart')
    cleanup = rows(text, 'mu4_cleanup_ingress')
    cleanup = [row for row in cleanup if row['address'] == '02956f']
    first_cleanup = text.index('mu4_cleanup_ingress: ordinal=' + cleanup[0]['ordinal'] + ' ') if cleanup else -1
    cleanup_write = next(line for line in text.splitlines()
                         if 'mu4_playback_lifecycle_write:' in line
                         and 'address=373e value=0005 ' in line)
    if first_cleanup < 0 or first_cleanup >= text.index(cleanup_write):
        raise ValueError('missing ingress before cleanup lifecycle write')
    return {'lifecycle_values': [row['value'] for row in sequence],
            'times': [float(row['t']) for row in sequence],
            'resident_return_time': resident_time,
            'scope': 'Observed cleanup, resident re-entry and playback lifecycle restart; '
                     'not buffered-tail disposition, repeat policy or complete audio acceptance'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--input-file', type=Path,
                        help='Compare the bounded source model with an independently extracted input file')
    parser.add_argument('--first-resident-only',action='store_true',
                        help='explicitly select the complete first session before a second resident entry')
    parser.add_argument('--lifecycle-restart', action='store_true',
                        help='also require the observed cleanup/resident/playback restart chronology')
    args = parser.parse_args()
    try:
        text = args.log.read_text()
        result = check(text, args.input_file.read_bytes() if args.input_file else None,
                       first_resident_only=args.first_resident_only)
        if args.lifecycle_restart:
            result['lifecycle_restart'] = check_lifecycle_restart(text)
        print(json.dumps(result, indent=2))
    except (ValueError, KeyError, OSError) as error:
        print(f'cleanup ingress check: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
