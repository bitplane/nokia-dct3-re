"""Compare actual DMA reads to bounded buffer-write epochs, not audible output."""
import argparse
from collections import Counter
from decimal import Decimal, InvalidOperation
import json
from pathlib import Path
import sys

if __package__:
    from .mu4_playback_frame_check import rows
else:
    from mu4_playback_frame_check import rows


def check(text):
    epochs = rows(text, 'mu4_playback_dma_epoch')
    ends = rows(text, 'mu4_pcm_copy_terminal')
    if (len(ends) != 1 or ends[0]['closed'] != '1' or ends[0]['capped'] != '0'
            or ends[0]['illegal'] != '0' or Decimal(ends[0]['t']) < Decimal('1.25')
            or text.index('mu4_pcm_copy_terminal:') < text.rindex('mu4_playback_dma_epoch:')):
        raise ValueError('missing or incomplete bounded capture terminal')
    copies = {}
    for row in rows(text, 'mu4_pcm_copy_bus'):
        if row['kind'] == 'write':
            key = (int(row['address'], 16), Decimal(row['t']))
            if key in copies:
                raise ValueError('ambiguous copy-write epoch')
            copies[key] = row
    if not epochs or not copies:
        raise ValueError('missing DMA or copy observations')
    owners, copied, unknown, previous = Counter(), Counter(), 0, None
    for row in epochs:
        ordinal, address, word = int(row['read']), int(row['address'],16), int(row['word'],16)
        time, writer_time = Decimal(row['t']), Decimal(row['last_write_t'])
        writer = int(row['last_write_pc'],16)
        if (not time.is_finite() or not writer_time.is_finite()
                or not Decimal('1') <= time <= Decimal('1.25')
                or not 0x1000 <= address <= 0x137f or not 0 <= word <= 0xffff
                or row['channel'] != '3' or row['ownership'] != 'active_source_read'
                or row['observer_writes'] != '0' or row['audio_acceptance'] != '0'
                or (previous and (ordinal != previous[0]+1 or time < previous[1]))):
            raise ValueError('invalid or discontinuous actual DMA epoch')
        previous = ordinal,time
        if writer_time == -1:
            unknown += 1
            continue
        if (not Decimal('1') <= writer_time <= time or row['last_write_mask'] != 'ffff'
                or int(row['last_write_word'],16) != word or not 0 <= writer <= 0x7fffff):
            raise ValueError('DMA value differs from complete last observed write')
        owners[f'{writer:06x}'] += 1
        if writer == 0x2a367:
            key = address,writer_time
            source = copies.get(key)
            if source is None or int(source['word'],16) != word:
                raise ValueError('DMA copy origin lacks matching actual copy write')
            copied[key] += 1
    return dict(reads=len(epochs), first=epochs[0]['t'], last=epochs[-1]['t'],
                last_writer_contexts=dict(owners), unobserved_prior_writes=unknown,
                copy_origin_reads=sum(copied.values()), distinct_copy_words=len(copied),
                repeated_copy_reads=sum(count-1 for count in copied.values()),
                native_audio_validated=False,
                scope='Actual channel-3 reads and last observed data-space writes in 1.0..1.25 seconds; '
                      'not complete-track consumption, DXR acceptance, serial delivery or analog audio')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log',type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(check(args.log.read_text()),indent=2))
    except (ValueError,KeyError,OSError,InvalidOperation) as error:
        print(f'FAIL: {error}',file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
