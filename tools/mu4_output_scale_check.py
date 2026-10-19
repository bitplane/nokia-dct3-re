"""Evaluate source-pinned signed scaling from actual operands, not fitted gain."""
import argparse
from collections import Counter, deque
from decimal import Decimal
import json
from pathlib import Path
import sys

if __package__:
    from .mu4_context_contract import contract, reference_output_scale
    from .mu4_playback_frame_check import rows
else:
    from mu4_context_contract import contract, reference_output_scale
    from mu4_playback_frame_check import rows


def check(text):
    scope = dict(first='1.06',last='1.10',cap='100000',read_context='029fea..029ff1',
                 write_context='029fee',boundary='data_bus',observer_writes='0',audio_acceptance='0')
    events,ends = rows(text,'mu4_scale_bus'),rows(text,'mu4_scale_terminal')
    if (rows(text,'mu4_scale_scope') != [scope] or not events or len(ends) != 1
            or not len(events) == int(ends[0]['events']) < 100000
            or ends[0]['closed'] != '1' or ends[0]['capped'] != '0' or ends[0]['illegal'] != '0'
            or Decimal(ends[0]['t']) < Decimal('1.10')
            or text.index('mu4_scale_terminal:') < text.rindex('mu4_scale_bus:')):
        raise ValueError('incomplete or changed scale capture')
    copies = {(int(row['address'],16),Decimal(row['t'])):int(row['word'],16)
              for row in rows(text,'mu4_pcm_copy_bus') if row['kind'] == 'write'}
    def bus_key(row):
        return Decimal(row['t']),int(row['address'],16),int(row['word'],16)
    dma_keys = Counter(bus_key(row) for row in rows(text,'mu4_playback_dma_epoch')
                       if row['channel'] == '3' and row['ownership'] == 'active_source_read')
    read_keys = Counter(bus_key(row) for row in events if row['kind'] == 'read')
    for key in read_keys.keys() & dma_keys.keys():
        if read_keys[key] != dma_keys[key]:
            raise ValueError('CPU/DMA timestamp-address-word collision has ambiguous ownership')
    pending,coefficients,origins,outputs = deque(),Counter(),Counter(),{}
    previous,changed,nonzero,censored,excluded_dma = Decimal('1.06'),0,0,0,0
    for ordinal,row in enumerate(events,1):
        address,word,pc,st1,pmst = (int(row[key],16) for key in ('address','word','context_pc','st1','pmst'))
        time = Decimal(row['t'])
        if (int(row['ordinal']) != ordinal or not previous <= time <= Decimal('1.10')
                or not 0 <= address <= 0xffff or not 0 <= word <= 0xffff or row['mask'] != 'ffff'
                or not st1 & 0x100 or st1 & 0x240 or not pmst & 1):
            raise ValueError('invalid scale bus event, modes or chronology')
        previous = time
        if row['kind'] == 'read':
            if not 0x29fea <= pc <= 0x29ff1:
                raise ValueError('scale operand outside helper context')
            if bus_key(row) in dma_keys:
                excluded_dma += 1
                continue
            pending.append(row)
            continue
        if row['kind'] != 'write' or pc != 0x29fee or not 0x1000 <= address <= 0x137f:
            raise ValueError('invalid scale output')
        if len(pending) < 2:
            if outputs or censored:
                raise ValueError('scale output lacks two actual input words')
            censored += 1
            pending.clear()
            continue
        sample,coefficient = pending.popleft(),pending.popleft()
        value,gain = int(sample['word'],16),int(coefficient['word'],16)
        if reference_output_scale(value,gain) != word:
            raise ValueError(f'actual scale store differs from independent signed Q12 result '
                             f'at event {ordinal}: sample={value:04x} coefficient={gain:04x} '
                             f'actual={word:04x} expected={reference_output_scale(value,gain):04x}')
        writer_time = Decimal(sample['last_write_t'])
        writer = int(sample['last_write_pc'],16)
        if (sample['last_write_mask'] != 'ffff' or int(sample['last_write_word'],16) != value
                or not Decimal('1') <= writer_time <= Decimal(sample['t'])):
            raise ValueError('scale source lacks complete last-write correspondence')
        if writer == 0x2a367:
            if copies.get((int(sample['address'],16),writer_time)) != value:
                raise ValueError('scale sample lacks matching copy-write epoch')
            origins['copy'] += 1
        else:
            origins[f'other_{writer:06x}'] += 1
        coefficients[f'{gain:04x}'] += 1
        changed += value != word
        nonzero += word != 0
        if (address,time) in outputs:
            raise ValueError('duplicate scale output epoch')
        outputs[address,time] = word
    if len(pending) > 2:
        raise ValueError('unmatched scale operands exceed one boundary sample')
    processing_reads = 0
    for row in rows(text,'mu4_processing_bus'):
        if row['kind'] != 'read' or row.get('last_write_pc') != '029fee':
            continue
        key = int(row['address'],16),Decimal(row['last_write_t'])
        if key not in outputs:
            continue
        if outputs[key] != int(row['word'],16):
            raise ValueError('processing input differs from actual scale output')
        processing_reads += 1
    return dict(outputs=len(outputs),nonzero_outputs=nonzero,changed_samples=changed,
                coefficients=dict(coefficients),input_origins=dict(origins),
                processing_reads_matched=processing_reads,left_censored_outputs=censored,
                excluded_actual_dma_reads=excluded_dma,
                right_censored_operands=len(pending),native_audio_validated=False,
                scope='Independent signed scalar arithmetic and actual copy/scale/processing epochs; '
                      'not coefficient policy, independent filter arithmetic, serial or analog acceptance')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log',type=Path)
    parser.add_argument('--source-a00',type=Path,required=True)
    args = parser.parse_args()
    try:
        contract(args.source_a00.read_bytes())
        print(json.dumps(check(args.log.read_text()),indent=2))
    except (ValueError,KeyError,OSError,ArithmeticError) as error:
        print(f'FAIL: {error}',file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
