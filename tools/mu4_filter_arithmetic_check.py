"""Independent bounded AA99 fixed-point kernel equations, not codec acceptance."""
import argparse
from collections import Counter, deque
from decimal import Decimal
import json
from pathlib import Path
import sys

if __package__:
    from .mu4_context_contract import contract
    from .mu4_playback_frame_check import rows
else:
    from mu4_context_contract import contract
    from mu4_playback_frame_check import rows


def signed(word):
    return word-65536 if word & 0x8000 else word


def wrap40(value):
    return ((value+(1 << 39)) % (1 << 40))-(1 << 39)


def stored32(value):
    return max(-(1 << 31),min((1 << 31)-1,value)) & 0xffffffff


def reference_iteration(r, *, fractional=False):
    """SPRU172C MAC/MACSU/MPYU and shifts; OVM=0, SXM=1, SST=1.

    The 25 words are actual bus reads in instruction order. The adjacent
    high-state consumer instead uses the derived pending store (SPRU131G 7.3.2).
    """
    if len(r) != 25 or any(not 0 <= word <= 0xffff for word in r):
        raise ValueError('filter iteration requires 25 16-bit bus operands')
    factor = 2 if fractional else 1
    a = signed(r[0]) << 14
    a = wrap40(a+signed(r[1])*signed(r[2])*factor)
    b = wrap40(r[3]*signed(r[4])*factor)
    a = wrap40(a+signed(r[5])*signed(r[6])*factor)
    b = wrap40(b+r[7]*signed(r[8])*factor)
    a,b = a >> 1,b >> 1
    b = wrap40(b+r[9]*signed(r[10])*factor)
    a = wrap40(a+signed(r[11])*signed(r[12])*factor)
    b = wrap40(b+r[13]*signed(r[14])*factor)
    b = wrap40(b+r[15]*signed(r[16])*factor)
    a = wrap40(a+signed(r[17])*signed(r[18])*factor)
    b = wrap40(b+r[19]*signed(r[20])*factor)
    a = wrap40(wrap40(a+(b >> 16)) << 2)
    state = stored32(wrap40(-a))
    low,high = state & 0xffff,state >> 16
    if r[22] != low:
        raise ValueError('actual low-state read differs from independent state update')
    # The MPY high operand is forwarded, not a separately observed RAM read.
    b = wrap40(signed(r[21])*signed(high)*factor)
    a = wrap40(a+(b << 4))
    b = wrap40(r[21]*r[22]*factor)
    a = wrap40(a+(b >> 12))
    a = wrap40(a+(signed(r[23]) << 15))
    b = wrap40(signed(r[21])*signed(r[24])*factor)
    a = wrap40(a+(b << 3))
    return [r[1],low,high,stored32(a) >> 16]


def check(text, *, section_words=64):
    if not 1 <= section_words <= 64:
        raise ValueError('section coverage must be one to 64 samples')
    scope = dict(first='1.061',last='1.062',cap='50000',first_address='1000',last_address='13ff',
                 context='02a2c6..02a2eb',boundary='data_bus',observer_writes='0',audio_acceptance='0')
    events,ends = rows(text,'mu4_filter_bus'),rows(text,'mu4_filter_terminal')
    if (rows(text,'mu4_filter_scope') != [scope] or not events or len(ends) != 1
            or not len(events) == int(ends[0]['events']) < 50000 or ends[0]['closed'] != '1'
            or ends[0]['capped'] != '0' or ends[0]['illegal'] != '0'
            or Decimal(ends[0]['t']) < Decimal('1.062')
            or text.index('mu4_filter_terminal:') < text.rindex('mu4_filter_bus:')):
        raise ValueError('missing, capped or incomplete filter capture')
    def key(row):
        return Decimal(row['t']),int(row['address'],16),int(row['word'],16)
    dma = Counter(key(row) for row in rows(text,'mu4_playback_dma_epoch')
                  if row['channel'] == '3' and row['ownership'] == 'active_source_read')
    reads = Counter(key(row) for row in events if row['kind'] == 'read')
    if any(reads[k] != dma[k] for k in reads.keys() & dma.keys()):
        raise ValueError('ambiguous CPU/DMA filter-operand collision')
    pending,writes = deque(),[]
    completed,excluded_dma,excluded_prefix,nonzero_state = 0,0,0,0
    sections,direct,section_samples = Counter(),Counter(),{}
    previous = Decimal('1.061')
    for ordinal,row in enumerate(events,1):
        time,address,word = key(row)
        if (int(row['ordinal']) != ordinal or not previous <= time <= Decimal('1.062')
                or row['mask'] != 'ffff' or not 0x1000 <= address <= 0x13ff or not 0 <= word <= 0xffff
                or not 0x2a2c6 <= int(row['context_pc'],16) <= 0x2a2eb):
            raise ValueError('invalid filter bus event')
        previous = time
        if row['kind'] == 'read':
            if key(row) in dma:
                excluded_dma += 1
            else:
                pending.append(row)
            continue
        if row['kind'] != 'write':
            raise ValueError('unknown filter bus event')
        if not writes:
            # Identify the source-pinned LD-state/LD-sample/MAC-coefficient triple
            # from actual addresses, not a guessed Execute PC.
            starts = [i for i in range(len(pending)-2)
                      if int(pending[i]['address'],16) == address
                      and 0x1000 <= int(pending[i+1]['address'],16) <= 0x10ff
                      and int(pending[i+1]['word'],16) == word
                      and int(pending[i+2]['address'],16) == (address & ~1)+4]
            if len(starts) != 1 or (starts[0] and completed % section_words):
                raise ValueError('ambiguous or missing filter iteration start')
            for _ in range(starts[0]):
                prefix = pending.popleft()
                if prefix['context_pc'] != '02a2c7':
                    raise ValueError('unclassified read before filter iteration')
                excluded_prefix += 1
        writes.append(row)
        if not 0x1000 <= address <= 0x10ff:
            continue
        if len(writes) != 4 or len(pending) != 25:
            raise ValueError(f'filter iteration has {len(pending)} reads/{len(writes)} writes, not 25/4')
        operands = list(pending)
        words = [int(operand['word'],16) for operand in operands]
        addresses = [int(operand['address'],16) for operand in operands]
        state,sample = addresses[:2]
        base = state & ~1
        c,high,low = base+4,base+8+(state & 1),base+12+(state & 1)
        expected_addresses = [state,sample,c,c+1,state,c+2,state^1,c+3,state^1,
                              c+3,high,c+2,high,low,c+2,c+1,high^1,c,high^1,
                              low^1,c,c-1,low^1,sample,sample]
        if addresses != expected_addresses or [int(w['address'],16) for w in writes] != [state,low^1,high^1,sample]:
            raise ValueError('filter operands/stores violate source-pinned addressing geometry')
        if (Decimal(writes[0]['t']) > Decimal(operands[4]['t'])
                or Decimal(writes[1]['t']) > Decimal(operands[22]['t'])):
            raise ValueError('filter dependent read precedes the state publication')
        modes = {(int(item['st1'],16) & 0x340,int(item['pmst'],16) & 1) for item in operands+writes}
        if len(modes) != 1 or next(iter(modes)) not in ((0x100,1),(0x140,1)):
            raise ValueError('unsupported/changing filter arithmetic modes')
        expected = reference_iteration(words,fractional=bool(int(row['st1'],16) & 0x40))
        actual = [int(w['word'],16) for w in writes]
        if expected != actual:
            raise ValueError(f'independent filter arithmetic mismatch at iteration {completed+1}: {expected} != {actual}')
        completed += 1
        nonzero_state += any(actual[1:3])
        sections[f'{base:04x}'] += 1
        section_samples.setdefault(f'{base:04x}',[]).append(sample)
        direct[f'{words[21]:04x}'] += 1
        pending.clear()
        writes.clear()
    if pending or writes or not completed or any(count != section_words for count in sections.values()):
        raise ValueError('incomplete section or unconsumed filter traffic')
    for samples in section_samples.values():
        if samples[0] & 63 or samples != list(range(samples[0],samples[0]+section_words)):
            raise ValueError('section does not cover its consecutive aligned sample block')
    return dict(iterations=completed,section_words=section_words,sections=dict(sections),direct_coefficients=dict(direct),
                sample_blocks={section:f'{samples[0]:04x}..{samples[-1]:04x}' for section,samples in section_samples.items()},
                nonzero_state_iterations=nonzero_state,excluded_actual_dma_reads=excluded_dma,
                boundary_context_reads=excluded_prefix,modeled_forwarded_high_operands=completed,
                actual_bus_reads=completed*25,actual_writes_checked=completed*4,
                native_audio_validated=False,
                scope='Independent bounded fixed-point state/sample equations from source-pinned operand geometry; '
                      'forwarded high operands derived, not bus-observed; not arbitrary coefficients or analog acceptance')


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
