"""Bind converted samples to actual copy reads/writes, not downstream DMA epochs."""
import argparse
from bisect import bisect_right
from collections import Counter, deque
import json
import math
from pathlib import Path
import sys

if __package__:
    from .mu4_playback_frame_check import rows
    from .mu4_context_contract import contract
    from .mu4_coefficient_check import verify as verify_arithmetic
else:
    from mu4_playback_frame_check import rows
    from mu4_context_contract import contract
    from mu4_coefficient_check import verify as verify_arithmetic


def check(text, *, frames=8):
    if not 1 <= frames <= 8:
        raise ValueError('copy proof covers one to eight fully observed conversion blocks')
    scope = dict(first='1.0',last='1.25',cap='65536',source_first='7000',source_last='7fff',
        destination_first='1000',destination_last='137f',copy_objects='13e0,1460',
        read_context='02a31f..02a371',write_context='02a367',source_write_tracking='last_actual',
        boundary='data_bus',context_pc='not_owner',observer_writes='0',audio_acceptance='0')
    events,ends = rows(text,'mu4_pcm_copy_bus'),rows(text,'mu4_pcm_copy_terminal')
    if rows(text,'mu4_pcm_copy_scope') != [scope] or len(ends) != 1 or not events or '\0' in text or 'LUA ERROR' in text:
        raise ValueError('missing, changed or failed PCM copy scope')
    try:
        end = ends[0]
        if (not len(events) == int(end['events']) < 65536 or end['closed'] != '1'
                or end['capped'] != '0' or end['illegal'] != '0' or float(end['t']) < 1.25
                or not math.isfinite(float(end['t'])) or end['observer_writes'] != '0' or end['audio_acceptance'] != '0'):
            raise ValueError('incomplete, capped or faulted copy capture')
        block_rows = rows(text,'mu4_conversion_block')
        blocks = {int(row['call']):row for row in block_rows}
        if len(blocks) != len(block_rows):
            raise ValueError('duplicate converter blocks')
        values = {(int(row['call']),int(row['destination'],16)):row for row in rows(text,'mu4_conversion_value')}
        if len(values) != len(rows(text,'mu4_conversion_value')):
            raise ValueError('duplicate converter destinations')
        for call in range(1,frames+1):
            block = blocks[call]
            base = int(block['destination'],16)
            if (block['bad'] != '0' or block['samples'] != '1024' or block['identity'] != 'Execute_entry'
                    or base not in (0x7000,0x7800) or int(block['frame']) != call-1
                    or block['observer_writes'] != '0' or block['audio_acceptance'] != '0'):
                raise ValueError('incomplete selected converter block')
            selected = [row for (which,_),row in values.items() if which == call]
            if (len(selected) != 2048 or
                    {(int(row['sample']),int(row['channel'])) for row in selected} !=
                    {(sample,channel) for sample in range(1024) for channel in range(2)}):
                raise ValueError('incomplete selected converter values')
            for row in selected:
                if (int(row['destination'],16) != base+int(row['sample'])+1024*int(row['channel'])
                        or int(row['frame']) != call-1 or not 0 <= int(row['result'],16) < 1<<40
                        or row['identity'] != 'Execute_entry' or row['observer_writes'] != '0'
                        or row['retirement'] != 'unclaimed' or row['argument'] != '14'):
                    raise ValueError('converter publication geometry or scope changed')
        returns = [row for row in rows(text,'mu4_decoder_call') if row['phase'] == 'return']
        times = [float(row['t']) for row in returns]
        if not times or any(not math.isfinite(t) for t in times) or any(b <= a for a,b in zip(times,times[1:])):
            raise ValueError('missing or unordered decoder return times')
        if any(int(row['call']) != index+1 or row['identity'] != 'Execute_entry'
               or row['context_only'] != '1' or row['observer_writes'] != '0'
               for index,row in enumerate(returns)):
            raise ValueError('decoder return identity changed')
        if any(not 0 <= int(row['result'],16) < 1<<40 or int(row['result'],16)&0xffff
               for row in returns if int(row['call']) <= frames):
            raise ValueError('selected decoder call did not return success')
        pending, covered, chunks = deque(),Counter(),{}
        counts,other,previous = Counter(),0,1.0
        for ordinal,event in enumerate(events,1):
            kind,when = event['kind'],float(event['t'])
            address,word,pc = (int(event[key],16) for key in ('address','word','context_pc'))
            if (int(event['ordinal']) != ordinal or kind not in ('read','write') or
                    not previous <= when <= 1.25 or not math.isfinite(when) or event['mask'] != 'ffff'
                    or not 0 <= word <= 0xffff or not 0 <= pc <= 0x7fffff or event['observer_writes'] != '0'
                    or int(event['ar4'],16) not in (0x13e0,0x1460)
                    or any(not 0 <= int(event[key],16) <= 0xffff for key in ('ar1','ar2','ar3'))):
                raise ValueError('copy bus identity/domain/order changed')
            previous = when
            counts[kind] += 1
            if kind == 'read':
                if not 0x7000 <= address <= 0x7fff or not 0x2a31f <= pc <= 0x2a371:
                    raise ValueError('read outside source/helper scope')
                writer_time = float(event['last_write_t'])
                writer_pc = int(event['last_write_pc'],16)
                if (event['last_write_mask'] != 'ffff' or int(event['last_write_word'],16) != word
                        or not 1 <= writer_time <= when or not math.isfinite(writer_time)
                        or not 0x23e1d <= writer_pc <= 0x23e2c):
                    raise ValueError('source has no corresponding last observed conversion write')
                at = bisect_right(times,writer_time)-1
                if at < 0:
                    raise ValueError('source write precedes every decoder return')
                call = int(returns[at]['call'])
                item = None
                if call <= frames:
                    value = values[(call,address)]
                    sample,channel = int(value['sample']),int(value['channel'])
                    if (int(value['result'],16)&0xffff != word or not float(value['t']) <= writer_time
                            or not float(blocks[call]['t']) <= when):
                        raise ValueError('copied source differs from its completed converter publication')
                    item = (call,channel,sample)
                pending.append((event,item))
            else:
                if not 0x1000 <= address <= 0x137f or pc != 0x2a367 or not pending:
                    raise ValueError('copy write has no chronological source read')
                source,item = pending.popleft()
                if event['word'] != source['word'] or event['ar4'] != source['ar4']:
                    raise ValueError('copy write differs from its chronological source read')
                if item is None:
                    other += 1
                    continue
                call,channel,sample = item
                base = address-(sample%64)
                if base not in (0x1000+64*channel,0x1080+64*channel):
                    raise ValueError('copy destination differs from its channel/chunk geometry')
                chunk = (call,channel,sample//64)
                if chunks.setdefault(chunk,base) != base:
                    raise ValueError('destination bank changes inside a copied chunk')
                covered[item] += 1
        expected = {(call,channel,sample) for call in range(1,frames+1) for channel in range(2) for sample in range(1024)}
        if set(covered) != expected or any(count != 1 for count in covered.values()):
            raise ValueError('selected converted blocks lack exactly-once copy coverage')
        census = rows(text,'mu4_pcm_destination_census')
        contexts = [int(row['context_pc'],16) for row in census]
        if (counts['read'] != int(end['reads']) or counts['write'] != int(end['writes'])
                or len(set(contexts)) != len(contexts) or len(census) != int(end['contexts'])
                or any(not 0 <= pc <= 0x7fffff for pc in contexts)
                or any(int(row['writes']) <= 0 or row['observer_writes'] != '0' for row in census)
                or sum(int(row['writes']) for row in census) != int(end['destination_writes'])
                or int(end['destination_writes']) < counts['write']
                or int(end['source_writes']) < len(covered)):
            raise ValueError('copy/census terminal totals differ')
        if text.index('mu4_pcm_copy_terminal:') < text.rindex('mu4_pcm_copy_bus:'):
            raise ValueError('copy terminal precedes bus observations')
        alias_writes = int(end['program_alias_writes']) if 'program_alias_writes' in end else None
        if alias_writes is not None and not 0 <= alias_writes <= int(end['source_writes']):
            raise ValueError('invalid forwarded program-alias write count')
    except (KeyError,TypeError,OverflowError) as error:
        raise ValueError('incomplete copy/conversion observation fields') from error
    return dict(blocks=frames,covered_words=len(covered),chunks=len(chunks),
        captured_reads=counts['read'],captured_copy_writes=counts['write'],
        writes_outside_selected_blocks=other,right_censored_reads=len(pending),
        source_writes_tracked=int(end['source_writes']),destination_writes_counted=int(end['destination_writes']),
        destination_contexts=len(census),fitted_alignment=False,native_audio_validated=False,
        program_alias_writes=alias_writes,
        scope='Converted-word correspondence to last observed data-space bank writes and chronological copy reads/writes; '
              'not retained instruction ownership, downstream DMA epochs, pin ownership, whole-track or analog audio acceptance')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--source-a00',type=Path,required=True)
    args = parser.parse_args()
    try:
        source = args.source_a00.read_bytes()
        contract(source)
        result = check((args.directory/'error.log').read_text())
        result['bounded_arithmetic_validated'] = verify_arithmetic(args.directory,source)['coefficient_publication_validated']
    except (ValueError,OSError) as error:
        parser.exit(1,f'MU4 PCM copy: {error}\n')
    print(json.dumps(result,indent=2))


if __name__ == '__main__':
    main()
