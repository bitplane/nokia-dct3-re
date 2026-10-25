"""Validate bounded decoder branch observations, not EOF or audio acceptance."""
import argparse
from collections import Counter
import json
from pathlib import Path
import re
import sys

if __package__:
    from .mu4_playback_frame_check import rows
    from .mu4_context_contract import reference_decoder_return_route, reference_worker_reservation, CONTEXT_LANE_CALLS
else:
    from mu4_playback_frame_check import rows
    from mu4_context_contract import reference_decoder_return_route, reference_worker_reservation, CONTEXT_LANE_CALLS

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


def check_counter_pointers(text):
    report = check_counter_producers(text)
    pointers = rows(text,'mu4_context_pointer_read')
    terminals = rows(text,'mu4_context_pointer_terminal')
    increments = [row for row in rows(text,'mu4_context_count_read')
                  if (row['instruction'],row['opcode']) == ('0201ca','6b83')]
    if len(terminals)!=1 or not pointers:
        raise ValueError('missing or unmatched counter pointer reads')
    end = terminals[0]
    if (int(end['reads'])!=len(pointers) or len(pointers)>256 or end['capped']!='0'
            or end['identity']!='Access_bus' or end['observer_writes']!='0'
            or (float(end['first']),float(end['last']))!=tuple(report['window'])
            or float(end['t'])<float(end['last'])):
        raise ValueError('incomplete counter pointer window')
    previous = report['window'][0]
    sources,links = Counter(),Counter()
    for ordinal,pointer in enumerate(pointers,1):
        when = float(pointer['t'])
        if (int(pointer['ordinal'])!=ordinal or not previous<=when<=report['window'][1]
                or (pointer['instruction'],pointer['opcode'])!=('0201c7','7192')
                or not 0x60<=int(pointer['word'],16)<=0xffff or pointer['mask']!='ffff'
                or pointer['identity']!='Access_bus' or pointer['observer_writes']!='0'
                or not 0x60<=int(pointer['address'],16)<=0xffff):
            raise ValueError('counter pointer ownership, target or chronology changed')
        previous = when
        links[pointer['address'],pointer['word']] += 1
        if pointer['word']=='d7ac':
            sources[pointer['address']] += 1
    selected = [pointer for pointer in pointers if pointer['word']=='d7ac']
    if len(selected)!=len(increments):
        raise ValueError('unmatched counter pointer reads')
    previous = report['window'][0]
    for pointer,increment in zip(selected,increments):
        if not previous<=float(pointer['t'])<float(increment['t'])<=report['window'][1]:
            raise ValueError('counter pointer and increment chronology changed')
        previous = float(increment['t'])
    report['counter_pointers'] = dict(reads=len(pointers),target_reads=len(selected),sources=dict(sorted(sources.items())),
        links=[dict(source=source,target=target,reads=count) for (source,target),count in sorted(links.items())],
        scope='Actual Address-stage MVDK reads preceding count increments; '
              'not descriptor initialization ownership, write retirement, physical capacity or EOF policy')
    return report


def check_context_callers(text):
    report = check_counter_pointers(text)
    events = rows(text,'mu4_context_call')
    terminals = rows(text,'mu4_context_call_terminal')
    if len(terminals)!=1 or not events:
        raise ValueError('missing context call window')
    end = terminals[0]
    if (int(end['events'])!=len(events) or len(events)>1024 or end['bad']!='0' or end['capped']!='0'
            or (float(end['first']),float(end['last']))!=tuple(report['window'])
            or float(end['t'])<report['window'][1] or end['identity']!='Execute_entry'
            or end['operands']!='context_only' or end['observer_writes']!='0'):
        raise ValueError('incomplete context call window')
    calls = {site:0x20000+target for target,sites in CONTEXT_LANE_CALLS.items() for site in sites}
    returns = {0x20453:0x2044a,0x20460:0x20454,0x2046a:0x20461,0x20477:0x2046b}
    previous = report['window'][0]
    for ordinal,event in enumerate(events,1):
        address,kind = int(event['address'],16),event['kind']
        expected = 0xf982 if kind=='call' and address in calls else 0xf6be if kind=='entry' and address in calls.values() else 0xf4e4 if kind=='return' and address in returns else None
        when = float(event['t'])
        if (int(event['ordinal'])!=ordinal or not previous<=when<=report['window'][1]
                or int(event['opcode'],16)!=expected or event['identity']!='Execute_entry'
                or event['operands']!='context_only' or event['observer_writes']!='0'):
            raise ValueError('context call identity or chronology changed')
        previous = when
    pointers = rows(text,'mu4_context_pointer_read')
    stream = sorted([(float(event['t']),False,event) for event in events]+
                    [(float(event['t']),True,event) for event in pointers],key=lambda item:item[0])
    stack,pending,owners = [],None,Counter()
    inventory = Counter()
    target_intervals = []
    matched,prefix = 0,0
    for _,pointer,event in stream:
        if pointer:
            if event['word']!='d7ac':
                continue
            if not stack:
                raise ValueError('target pointer outside observed context wrapper')
            frame = stack[-1]
            offset = {0x20454:24,0x2046b:8}.get(frame['wrapper'])
            if offset is None or int(event['address'],16)!=((frame['base']+offset)&0xffff):
                raise ValueError('target pointer disagrees with wrapper address walk')
            owners[frame['caller'],frame['wrapper'],frame['base']] += 1
            frame['target_reads'] = frame.get('target_reads',0)+1
            continue
        address,kind = int(event['address'],16),event['kind']
        if kind=='call':
            if pending is not None:
                raise ValueError('context call without matching entry')
            pending = address
        elif kind=='entry':
            if pending is None or calls[pending]!=address:
                raise ValueError('context wrapper entry without matching caller')
            stack.append(dict(caller=pending,wrapper=address,base=int(event['a'],16)&0xffff,
                              entry_time=float(event['t'])))
            inventory[pending,address,stack[-1]['base']] += 1
            pending = None
            matched += 1
        elif not stack and not matched and pending is None:
            prefix += 1
        elif not stack or returns[address]!=stack[-1]['wrapper'] or pending is not None:
            raise ValueError('context wrapper return order changed')
        else:
            frame = stack.pop()
            if frame.get('target_reads'):
                target_intervals.append(dict(caller=f"{frame['caller']:06x}",
                    entry_time=frame['entry_time'],return_time=float(event['t']),reads=frame['target_reads']))
    if pending is not None or stack or not owners:
        raise ValueError('censored context wrapper or missing target ownership')
    report['context_callers'] = dict(events=len(events),matched_calls=matched,leading_returns=prefix,
        target_intervals=target_intervals,
        calls=[dict(caller=f'{caller:06x}',wrapper=f'{wrapper:06x}',context=f'{base:04x}',count=count)
               for (caller,wrapper,base),count in sorted(inventory.items())],
        target_owners=[dict(caller=f'{caller:06x}',wrapper=f'{wrapper:06x}',context=f'{base:04x}',reads=count)
                       for (caller,wrapper,base),count in sorted(owners.items())],
        scope='Observed call/entry/return intervals and actual pointer-read agreement with the source address walk; '
              'entry registers remain context snapshots, not retired operands; no EOF policy or physical units')
    return report


def check_exchange_status(text):
    report = check_context_callers(text)
    reads,events,ends = (rows(text,tag) for tag in
        ('mu4_exchange_status_read','mu4_exchange_branch','mu4_exchange_terminal'))
    if len(ends)!=1 or not reads or not events:
        raise ValueError('missing exchange status window')
    end = ends[0]
    if (int(end['reads'])!=len(reads) or int(end['events'])!=len(events)
            or len(reads)>256 or len(events)>512 or end['bad']!='0' or end['capped']!='0'
            or (float(end['first']),float(end['last']))!=tuple(report['window'])
            or float(end['t'])<report['window'][1] or end['observer_writes']!='0'
            or end['identity']!='Access_bus_and_Execute_entry'):
        raise ValueError('incomplete exchange status window')
    sites = {0x20553:0xf845,0x20555:0x10f8,0x20580:0x4811,0x20586:0x10f8}
    for items,kind in ((reads,'read'),(events,'branch')):
        previous = report['window'][0]
        for ordinal,event in enumerate(items,1):
            when = float(event['t'])
            if (int(event['ordinal'])!=ordinal or not previous<=when<=report['window'][1]
                    or event['observer_writes']!='0'):
                raise ValueError('exchange event chronology changed')
            if kind=='read':
                valid = (event['instruction']=='02a310' and event['opcode']=='10e1'
                    and event['mask']=='ffff' and event['identity']=='Access_bus'
                    and 0x60<=int(event['address'],16)<=0xffff and 0<=int(event['word'],16)<=0xffff)
            else:
                valid = (sites.get(int(event['address'],16))==int(event['opcode'],16)
                    and event['identity']=='Execute_entry' and event['operands']=='context_only')
            if not valid:
                raise ValueError('exchange status or branch identity changed')
            previous = when
    stream = sorted([(float(row['t']),True,row) for row in reads]+
                    [(float(row['t']),False,row) for row in events],key=lambda item:item[0])
    last_read,pending,phase,last_test = None,None,None,None
    publication_start = 0.0
    outcomes,flags = Counter(),Counter()
    unselected = 0
    publications = [row for row in rows(text,'mu4_context_call')
                    if row['kind']=='call' and row['address']=='020581']
    for when,is_read,event in stream:
        if is_read:
            last_read = event
            continue
        address = int(event['address'],16)
        if address==0x20553:
            if pending is not None or last_read is None:
                raise ValueError('exchange query without a complete predecessor')
            if (event['table']!='c6f0' or event['context']!='d796'
                    or int(event['receiver'],16)+9!=int(last_read['address'],16)
                    or event['field9']!=last_read['word'] or float(last_read['t'])>=when):
                raise ValueError('exchange getter does not agree with selected adapter')
            pending,phase = int(last_read['word'],16),'tested'
            last_test = dict(time=when,getter_time=float(last_read['t']),word=last_read['word'],
                receiver=event['receiver'],context_snapshots=dict(field13=event['field13'],
                    count22=event['count22'],flag1310=event['flag1310']))
            last_read = None
        elif phase=='tested':
            if address!=(0x20555 if pending else 0x20586):
                raise ValueError('exchange status branch successor changed')
            if pending:
                flags[event['flag1310']] += 1
                phase = 'eligible'
            else:
                outcomes['skip'] += 1
                pending,phase = None,None
        elif address==0x20580 and phase=='eligible':
            phase,publication_start = 'publishing',when
        elif address==0x20586 and phase=='publishing':
            intervals = report['context_callers']['target_intervals']
            if (sum(publication_start<float(row['t'])<when for row in publications)!=1
                    or sum(row['caller']=='020581' and publication_start<row['entry_time']<row['return_time']<when
                           for row in intervals)!=1):
                raise ValueError('exchange publication interval is incomplete')
            outcomes['publish'] += 1
            pending,phase = None,None
        elif address==0x20586 and pending is None:
            unselected += 1
        else:
            raise ValueError('exchange publication route changed')
    if pending is not None or not outcomes['skip'] or outcomes['publish']!=len(publications):
        raise ValueError('censored exchange route or absent branch control')
    report['exchange_status'] = dict(reads=len(reads),events=len(events),outcomes=dict(outcomes),
        publish_flags=dict(flags),unselected_common_entries=unselected,
        last_test=last_test,
        scope='Owned getter reads and observed successors/publication intervals in the bounded window; '
              'context snapshots are not retirement; no whole-track drain or EOF semantics')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--returns', action='store_true', help='require complete observed post-metadata return routes')
    parser.add_argument('--activity', action='store_true', help='also qualify counted-context reservation entry states')
    parser.add_argument('--counter-producers', action='store_true', help='also require actual CPU count increment/decrement operand reads')
    parser.add_argument('--counter-pointers', action='store_true', help='also require Address-stage counter pointer reads')
    parser.add_argument('--context-callers', action='store_true', help='also qualify wrapper call/entry/return ownership')
    parser.add_argument('--exchange-status', action='store_true', help='also qualify actual adapter getter reads and publication branches')
    args = parser.parse_args()
    try:
        text = args.log.read_text()
        checker = (check_exchange_status if args.exchange_status else check_context_callers if args.context_callers else check_counter_pointers if args.counter_pointers else check_counter_producers if args.counter_producers else check_activity_cycles
                   if args.activity else check_return_routes if args.returns else check)
        print(json.dumps(checker(text), indent=2))
    except (ValueError, KeyError, OSError) as error:
        print(f'decoder gate check: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
