"""Reconcile actual DRR read owners; CPU PC is context, not instruction ownership."""
import argparse
from decimal import Decimal
import json
from pathlib import Path
import sys

if __package__:
    from .mu4_playback_frame_check import rows
    from .noki5510_bootstrap_check import verify_recorder_input
else:
    from mu4_playback_frame_check import rows
    from noki5510_bootstrap_check import verify_recorder_input


def check(text):
    scopes,ends = (rows(text,marker) for marker in
        ('mu4_recorder_read_scope','mu4_recorder_read_terminal'))
    if scopes != [dict(address='0021',context_cap='128',dma_identity='active_source_read',
            context_pc='not_instruction_owner',side_effects_disabled='excluded',observer_writes='0')]:
        raise ValueError('missing or changed recorder read scope')
    if len(ends) != 1:
        raise ValueError('missing/repeated recorder read closure')
    end,source = ends[0],verify_recorder_input(text)
    stop = Decimal(end['t'])
    if (not stop.is_finite() or stop <= 0 or end['context_cap'] != '128'
            or end['save_restore'] != 'unsupported' or end['dropped'] != '0'):
        raise ValueError('incomplete or capped recorder context census')
    owners,contexts = {},{}
    for row in rows(text,'mu4_recorder_read_owner'):
        mask,count = int(row['dma_mask'],16),int(row['reads'])
        if mask in owners or not 0 <= mask < 64 or count <= 0:
            raise ValueError('invalid/repeated DMA owner histogram')
        owners[mask] = count
    for row in rows(text,'mu4_recorder_read_context'):
        pc,count = int(row['context_pc'],16),int(row['reads'])
        if pc in contexts or not 0 <= pc <= 0x7fffff or count <= 0:
            raise ValueError('invalid/repeated unowned-read context')
        contexts[pc] = count
    selected = sum(count for mask,count in owners.items() if mask&4)
    total = sum(owners.values())
    if (not selected or total != int(end['reads']) or not 0 <= len(contexts) == int(end['contexts']) <= 128
            or sum(contexts.values()) != owners.get(0,0)
            or selected != sum(source['dma_symbol_reads'])
            or total-selected != source['other_owner_reads']):
        raise ValueError('recorder owner/context/input counters do not reconcile')
    return {'reads':total,'stop_seconds':str(stop),
            'dma_masks':{f'{mask:02x}':count for mask,count in sorted(owners.items())},
            'unowned_contexts':{f'{pc:06x}':count for pc,count in sorted(contexts.items())},
            'multi_owner_masks':[f'{mask:02x}' for mask in owners if mask&(mask-1)],
            'channel2_selected_reads':selected,'unowned_bus_reads':owners.get(0,0),
            'encoder_consumption_validated':False,
            'scope':'Actual source-read API and bounded bus contexts; not CPU instruction ownership, encoder samples or complete recording acceptance'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log',type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(check(args.log.read_text()),indent=2))
    except (ValueError,KeyError,OSError,ArithmeticError) as error:
        parser.exit(1,f'MU4 recorder read census FAIL: {error}\n')


if __name__ == '__main__':
    main()
