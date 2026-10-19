"""Check passive original-controller buffer transitions, not EOF or audio acceptance."""
import argparse
from collections import Counter
import json
import math
from pathlib import Path
import sys

if __package__:
    from .mu4_playback_frame_check import rows
    from .mu4_context_contract import contract
else:
    from mu4_playback_frame_check import rows
    from mu4_context_contract import contract


def check(text):
    if '\0' in text or 'LUA ERROR' in text:
        raise ValueError('corrupt or failed observation')
    scope = rows(text,'mu4_buffer_scope')
    if scope != [dict(descriptor_first='0.825',last='10.0',max_nodes='12',max_writes='131072',
                      boundary='changed_data_write',identical_full_writes='deduplicated',
                      context_pc='not_owner',observer_writes='0')]:
        raise ValueError('missing or changed buffer observation scope')
    nodes = rows(text,'mu4_buffer_node')
    writes = rows(text,'mu4_buffer_write')
    closed = rows(text,'mu4_buffer_closed')
    terminal = rows(text,'mu4_buffer_terminal')
    try:
        if len(nodes) != 3 or not 0 < len(writes) < 131072 or len(closed) != 1 or len(terminal) != 1:
            raise ValueError('missing, duplicate or capped buffer capture')
        initial,values = {},{}
        for node in nodes:
            base = int(node['base'],16)
            words = [int(word,16) for word in node['words'].split(',')]
            when = float(node['t'])
            if len(words) != 12 or not all(0 <= value <= 0xffff for value in words) or not math.isfinite(when) or not 0.825 <= when < 1:
                raise ValueError('invalid initial descriptor snapshot')
            if base in initial:
                raise ValueError('duplicate descriptor')
            initial[base] = words
            values.update({base+i:value for i,value in enumerate(words)})
        if set(initial) != {0x160a,0x1616,0x1622} or any(initial[base][8:10] != links for base,links in
                ((0x1622,[0,0x1616]),(0x1616,[0x1622,0x160a]),(0x160a,[0x1616,0]))):
            raise ValueError('unexpected linked buffer topology')
        contexts,last_events = Counter(),{}
        previous_time,previous_bus = 0.0,0
        for ordinal,event in enumerate(writes,1):
            when = float(event['t'])
            bus = int(event['bus_ordinal'])
            base,address,value = (int(event[key],16) for key in ('base','address','value'))
            pc = int(event['context_pc'],16)
            if (int(event['ordinal']) != ordinal or bus <= previous_bus or event['mask'] != 'ffff'
                    or not math.isfinite(when) or not previous_time <= when <= 10.017
                    or not 0 <= value <= 0xffff or not 0 <= pc <= 0x7fffff):
                raise ValueError('changed buffer-write domain or ordering')
            if event['kind'] == 'descriptor':
                if base not in initial or not base <= address < base+12 or when < 0.825:
                    raise ValueError('write outside a captured descriptor')
            elif event['kind'] != 'control' or base or address not in (0x15e0,0x15df,0x12fb,0x12fc,0x1310):
                raise ValueError('unexpected buffer control')
            contexts[(base,address,pc)] += 1
            values[address] = value
            last_events[address] = dict(value=f'{value:04x}',t=when,context_pc=f'{pc:06x}')
            previous_time,previous_bus = when,bus
        count,bus_count = str(len(writes)),closed[0]['bus_writes']
        if (int(bus_count) < previous_bus or closed[0] != dict(writes=count,bus_writes=bus_count,capped='0',observer_writes='0')
                or terminal[0] != dict(closed='1',writes=count,bus_writes=bus_count,capped='0',illegal='0',observer_writes='0')):
            raise ValueError('incomplete or faulted buffer terminal')
        if text.index('mu4_buffer_closed:') > text.index('mu4_buffer_terminal:'):
            raise ValueError('buffer terminal precedes closure')
    except (KeyError,TypeError,OverflowError) as error:
        raise ValueError('incomplete buffer fields') from error
    return dict(changed_writes=len(writes),observed_bus_writes=int(bus_count),
                refill_context_changes=dict(requests=contexts[(0,0x15e0,0x2068e)],
                                            clears=contexts[(0,0x15e0,0x2088b)],
                                            context_only=True),
                topology={f'{base:04x}':[f'{word:04x}' for word in words[8:10]] for base,words in sorted(initial.items())},
                last_fields={f'{address:04x}':event for address,event in sorted(last_events.items())},
                contexts=[dict(base=f'{base:04x}',address=f'{address:04x}',context_pc=f'{pc:06x}',changes=count)
                          for (base,address,pc),count in sorted(contexts.items())],
                scope='Changed data writes and context PCs only; not retained instruction ownership, EOF completion or audio acceptance')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run',type=Path)
    parser.add_argument('--source-a00',required=True,type=Path)
    args = parser.parse_args()
    try:
        contract(args.source_a00.read_bytes())
        report = check((args.run/'error.log').read_text())
    except (OSError,ValueError) as error:
        print(f'MU4 buffer flow: {error}',file=sys.stderr)
        return 1
    print(json.dumps(report,indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
