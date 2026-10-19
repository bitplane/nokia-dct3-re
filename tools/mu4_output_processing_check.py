"""Trace copy/processing/DMA epochs without fitting or claiming filter arithmetic."""
import argparse
from collections import Counter
from decimal import Decimal
import json
from pathlib import Path
import sys

if __package__:
    from .mu4_playback_frame_check import rows
else:
    from mu4_playback_frame_check import rows


def check(text):
    scopes,ends = rows(text,'mu4_processing_scope'),rows(text,'mu4_processing_terminal')
    expected = dict(first='1.06',last='1.10',cap='100000',read_context='02a2c6..02a2ea',
                    read_address='ar7',write_context='02a2ea',boundary='data_bus',
                    observer_writes='0',audio_acceptance='0')
    events = rows(text,'mu4_processing_bus')
    if (scopes != [expected] or len(ends) != 1 or not events or len(events) >= 100000
            or ends[0]['closed'] != '1' or ends[0]['capped'] != '0' or ends[0]['illegal'] != '0'
            or int(ends[0]['events']) != len(events) or Decimal(ends[0]['t']) < Decimal('1.10')):
        raise ValueError('missing, capped or incomplete processing capture')
    if text.index('mu4_processing_terminal:') < text.rindex('mu4_processing_bus:'):
        raise ValueError('processing terminal precedes observed bus events')
    copies = {(int(row['address'],16),Decimal(row['t'])):int(row['word'],16)
              for row in rows(text,'mu4_pcm_copy_bus') if row['kind'] == 'write'}
    writes,latest_read = {},{}
    contexts,origins = Counter(),Counter()
    changed,nonzero,left_censored,previous_time = 0,0,0,Decimal('1.06')
    seen_reads = set()
    for ordinal,row in enumerate(events,1):
        address,word,pc = (int(row[key],16) for key in ('address','word','context_pc'))
        time = Decimal(row['t'])
        if (int(row['ordinal']) != ordinal or not previous_time <= time <= Decimal('1.10')
                or not 0x1000 <= address <= 0x137f or not 0 <= word <= 0xffff or row['mask'] != 'ffff'):
            raise ValueError('invalid processing event chronology or bus domain')
        previous_time = time
        if row['kind'] == 'read':
            if not 0x2a2c6 <= pc <= 0x2a2ea or int(row['ar7'],16) != address:
                raise ValueError('processing read outside declared sample context')
            contexts[f'{pc:06x}'] += 1
            writer_time = Decimal(row['last_write_t'])
            writer = int(row['last_write_pc'],16)
            if (row['last_write_mask'] != 'ffff' or int(row['last_write_word'],16) != word
                    or not Decimal('1') <= writer_time <= time):
                raise ValueError('processing read differs from last complete observed write')
            origin_key = address,writer_time
            if writer == 0x2a367:
                if copies.get(origin_key) != word:
                    raise ValueError('processing input lacks actual copy epoch')
                origins['copy'] += 1
            elif writer == 0x2a2ea:
                if writer_time >= Decimal('1.06') and writes.get(origin_key) != word:
                    raise ValueError('processing input lacks prior processing epoch')
                origins['processing'] += 1
            else:
                origins[f'other_{writer:06x}'] += 1
            latest_read[address] = time,word
            seen_reads.add(address)
        elif row['kind'] == 'write':
            if pc != 0x2a2ea or (address,time) in writes:
                raise ValueError('invalid or duplicate processing write')
            source = latest_read.pop(address,None)
            if source is None:
                if seen_reads or left_censored:
                    raise ValueError('processing write has no preceding sample read')
                left_censored += 1
            else:
                changed += source[1] != word
            nonzero += word != 0
            writes[address,time] = word
        else:
            raise ValueError('unknown processing bus event')
    if len(latest_read) > 1:
        raise ValueError('more than one sample lacks its final processing write')
    dma,unmatched = 0,0
    for row in rows(text,'mu4_playback_dma_epoch'):
        time,writer_time = Decimal(row['t']),Decimal(row['last_write_t'])
        if not Decimal('1.06') <= writer_time <= time <= Decimal('1.10'):
            continue
        if row['ownership'] != 'active_source_read' or row['channel'] != '3':
            raise ValueError('DMA observation lacks actual source ownership')
        key = int(row['address'],16),writer_time
        if key not in writes:
            unmatched += 1
            continue
        if (writes[key] != int(row['word'],16) or int(row['last_write_word'],16) != writes[key]
                or row['last_write_pc'] != '02a2ea' or row['last_write_mask'] != 'ffff'):
            raise ValueError('actual DMA read differs from processing output epoch')
        dma += 1
    return dict(sample_read_contexts=dict(contexts),input_origins=dict(origins),
                processing_writes=len(writes),changed_sample_writes=changed,nonzero_sample_writes=nonzero,
                left_censored_sample_writes=left_censored,
                dma_reads_matched=dma,other_bounded_dma_reads=unmatched,
                right_censored_sample_reads=len(latest_read),native_audio_validated=False,
                scope='Actual copy/processing/DMA dataflow only; independent filter arithmetic, '
                      'coefficient meaning, complete-track and analog output remain unvalidated')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log',type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(check(args.log.read_text()),indent=2))
    except (ValueError,KeyError,OSError,ArithmeticError) as error:
        print(f'FAIL: {error}',file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
