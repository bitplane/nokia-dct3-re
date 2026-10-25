"""Validate bounded decoder branch observations, not EOF or audio acceptance."""
import argparse
from collections import Counter
import json
from pathlib import Path
import re
import sys

if __package__:
    from .mu4_playback_frame_check import rows
    from .mu4_context_contract import reference_decoder_return_route, reference_worker_reservation
else:
    from mu4_playback_frame_check import rows
    from mu4_context_contract import reference_decoder_return_route, reference_worker_reservation

SITES = dict(zip(
    (0x2936d, 0x293a2, 0x293a8, 0x293aa, 0x293ad, 0x293ae,
     0x293b0, 0x293b5, 0x293b7, 0x293cd, 0x293cf, 0x293df,
     0x293e1, 0x293f1, 0x293f4, 0x293fe, 0x2940b),
    (0x10f8, 0x10f8, 0xf982, 0x8812, 0xf5aa, 0xf830,
     0x7211, 0xf845, 0x10f8, 0xf982, 0x8811, 0x6c81,
     0x76f8, 0x6ce1, 0x10f8, 0x10f8, 0x10f8)))


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


def check_return_routes(text):
    """Follow complete in-window calls, retaining an explicitly partial prefix."""
    result = check(text)
    pending, started, prefix = None, False, 0
    calls = []
    route_sites = {0x293cf, 0x293df, 0x293e1, 0x293f1, 0x293f4, 0x293fe}
    for event in rows(text, 'mu4_decoder_gate'):
        address = int(event['address'], 16)
        if pending is None:
            if address == 0x293cd:
                pending = dict(expected=0x293cf, processing_before=int(event['call']))
                started = True
            elif address in route_sites:
                if started:
                    raise ValueError('unmatched decoder return observation')
                prefix += 1
            continue
        if address != pending['expected']:
            raise ValueError('decoder return successor changed or missing')
        if address == 0x293cf:
            value = int(event['a'], 16)
            if not 0 <= value < 1 << 40:
                raise ValueError('decoder accumulator result outside 40 bits')
            pending.update(value=value & 0xffff, processing_after=int(event['call']), expected=0x293df)
        elif address == 0x293df:
            if int(event['ar1'], 16) != pending['value']:
                raise ValueError('metadata call did not retain decoder result')
            pending['expected'] = 0x293e1 if pending['value'] == 0 else 0x293f1
        elif address == 0x293f1:
            if int(event['ar1'], 16) != pending['value']:
                raise ValueError('second return branch changed retained result')
            pending['expected'] = 0x293f4 if pending['value'] == 2 else 0x293fe
        elif address in (0x293e1, 0x293f4):
            pending['expected'] = 0x293fe
        elif address == 0x293fe:
            pending['expected'] = 0x2940b
        else:
            calls.append(dict(ordinal=len(calls)+1, result=f'{pending["value"]:04x}',
                              route=reference_decoder_return_route(pending['value']),
                              processing_before=pending['processing_before'],
                              processing_after=pending['processing_after']))
            pending = None
    if pending is not None or not calls:
        raise ValueError('censored or absent complete decoder return route')
    result['return_routes'] = calls
    result['partial_prefix_route_events'] = prefix
    result['return_scope'] = ('Complete invocations inside the bounded window only; processing counters '
                              'are context, not attempt IDs; no EOF/drain or whole-session claim')
    return result


def check_activity_cycles(text):
    """Require bounded entry-state agreement, not complete queue ownership."""
    report = check_return_routes(text)
    events = rows(text, 'mu4_decoder_gate')
    starts = [i for i,event in enumerate(events) if int(event['address'],16)==0x293cd]
    if len(starts)!=len(report['return_routes']):
        raise ValueError('activity and complete-return coverage differ')
    gates = [i for i,event in enumerate(events) if int(event['address'],16)==0x293b5]
    for i in gates:
        state = reference_worker_reservation(int(events[i]['activity'],16),
                                            int(events[i]['context_activity'],16))
        passed = int(events[i+1]['address'],16)==0x293b7
        if state['eligible']!=passed:
            raise ValueError('activity branch and entry-state model differ')
    cycles,previous = [],-1
    for start,route in zip(starts,report['return_routes']):
        candidates = [i for i in gates if previous<i<start]
        if not candidates or route['result']!='0000':
            raise ValueError('missing acquisition context or changed zero-result control')
        gate = events[candidates[-1]]
        state = reference_worker_reservation(int(gate['activity'],16),int(gate['context_activity'],16))
        entered = events[start]
        refresh = next(event for event in events[start+1:] if int(event['address'],16)==0x293fe)
        contexts = [int(event['context'],16) for event in (gate,entered,refresh)]
        if not state['eligible'] or len(set(contexts))!=1 or not 0x60<=contexts[0]<=0xffe9:
            raise ValueError('activity context identity or eligibility changed')
        if (int(entered['activity'],16)!=state['flag_after_acquisition'] or
                int(entered['context_activity'],16)!=state['count_after_decrement'] or
                int(refresh['activity'],16)!=0):
            raise ValueError('reservation acquisition or zero-result flag release changed')
        cycles.append(dict(ordinal=len(cycles)+1,context=f'{contexts[0]:04x}',
                           before_flag=int(gate['activity'],16),before_count=int(gate['context_activity'],16),
                           call_flag=int(entered['activity'],16),call_count=int(entered['context_activity'],16),
                           refresh_flag=int(refresh['activity'],16),
                           refresh_count=int(refresh['context_activity'],16)))
        previous = start
    report['activity_cycles'] = cycles
    report['activity_scope'] = ('Observed entry-state agreement with the source prefix; helper callbacks '
                                'and indirect writers are not closed, physical queue capacity/units '
                                'and EOF/drain completion remain unqualified')
    return report


def check_counter_producers(text):
    """Identify actual operand-read owners; this does not observe their writes."""
    report = check_activity_cycles(text)
    reads = rows(text,'mu4_context_count_read')
    terminals = rows(text,'mu4_context_count_terminal')
    if len(terminals)!=1 or not reads:
        raise ValueError('missing counter-read inventory')
    end = terminals[0]
    if (int(end['reads'])!=len(reads) or len(reads)>256 or end['capped']!='0' or
            end['address']!='d7ac' or end['identity']!='Access_bus' or
            (float(end['first']),float(end['last']))!=tuple(report['window']) or
            float(end['t'])<float(end['last'])):
        raise ValueError('incomplete or changed counter-read scope')
    owners,increment,decrement = Counter(),[],[]
    previous = float(end['first'])
    for ordinal,event in enumerate(reads,1):
        when = float(event['t'])
        if (int(event['ordinal'])!=ordinal or not previous<=when<=float(end['last']) or
                event['address']!='d7ac' or event['mask']!='ffff' or
                event['identity']!='Access_bus' or not 0<=int(event['word'],16)<=0xffff):
            raise ValueError('counter-read identity or chronology changed')
        previous = when
        owner = event['instruction'],event['opcode']
        owners[owner] += 1
        if owner==('0201ca','6b83'):
            increment.append(int(event['word'],16))
        if owner==('020182','6b82'):
            decrement.append(int(event['word'],16))
    acquired_counts = [cycle['before_count'] for cycle in report['activity_cycles'] if cycle['before_flag']==0]
    if 0 not in increment or decrement!=acquired_counts:
        raise ValueError('counter producer or acquisition-read control changed')
    report['counter_reads'] = dict(total=len(reads),increment_reads=len(increment),
        increment_before_values=dict(sorted(Counter(increment).items())),
        decrement_reads=len(decrement),owners=[dict(instruction=pc,opcode=op,reads=count)
                                             for (pc,op),count in sorted(owners.items())],
        scope='Actual CPU operand-read ownership and bounded entry-state agreement; '
              'not write ownership, retirement, all producers, queue capacity, clocks or EOF completion')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--returns', action='store_true', help='require complete observed post-metadata return routes')
    parser.add_argument('--activity', action='store_true', help='also qualify counted-context reservation entry states')
    parser.add_argument('--counter-producers', action='store_true', help='also require actual CPU count increment/decrement operand reads')
    args = parser.parse_args()
    try:
        text = args.log.read_text()
        checker = (check_counter_producers if args.counter_producers else check_activity_cycles
                   if args.activity else check_return_routes if args.returns else check)
        print(json.dumps(checker(text), indent=2))
    except (ValueError, KeyError, OSError) as error:
        print(f'decoder gate check: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
