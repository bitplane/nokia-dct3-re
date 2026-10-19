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


def check_fresh(text,owners):
    ends = rows(text,'mu4_recorder_rx_terminal')
    if len(ends) != 1:
        raise ValueError('missing or repeated DRR publication closure')
    end = ends[0]
    if end['bad'] != '0' or end['save_restore'] != 'unsupported' or end['encoder_consumption'] != '0':
        raise ValueError('DRR publication lifetime/value observation failed')
    fresh,stale = {},{}
    for row in rows(text,'mu4_recorder_rx_owner'):
        mask = int(row['dma_mask'],16)
        new,old = int(row['fresh']),int(row['stale'])
        if mask in fresh or mask not in owners or min(new,old) < 0 or new+old != owners[mask]:
            raise ValueError('DRR read freshness does not match bus ownership')
        fresh[mask],stale[mask] = new,old
    publications,discarded,pending,resets,unknown = (int(end[field]) for field in
        ('publications','discarded','pending','resets','unknown_stale'))
    if (set(fresh) != set(owners) or min(publications,discarded,resets,unknown) < 0
            or pending not in (0,1) or unknown > sum(stale.values())
            or publications != sum(fresh.values())+discarded+pending):
        raise ValueError('DRR publication/reset/read counters do not reconcile')
    return {'publications':publications,'fresh_reads':sum(fresh.values()),
            'stale_reads':sum(stale.values()),'reset_discarded':discarded,'pending':pending,
            'unknown_stale_prefix_reads':unknown,
            'fresh_by_dma_mask':{f'{mask:02x}':value for mask,value in sorted(fresh.items())},
            'stale_by_dma_mask':{f'{mask:02x}':value for mask,value in sorted(stale.items())},
            'published_drr_delivery_validated':True,
            'scope':'Observed DRR publication/read readiness and retained-word equality; not ADC conversion, FIFO loss or encoder consumption'}


def check(text,require_fresh=False):
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
    freshness = None
    if rows(text,'mu4_recorder_rx_terminal'):
        freshness = check_fresh(text,owners)
    elif require_fresh:
        raise ValueError('DRR publication observation required')
    return {'reads':total,'stop_seconds':str(stop),'drr_freshness':freshness,
            'dma_masks':{f'{mask:02x}':count for mask,count in sorted(owners.items())},
            'unowned_contexts':{f'{pc:06x}':count for pc,count in sorted(contexts.items())},
            'multi_owner_masks':[f'{mask:02x}' for mask in owners if mask&(mask-1)],
            'channel2_selected_reads':selected,'unowned_bus_reads':owners.get(0,0),
            'encoder_consumption_validated':False,
            'scope':'Actual source-read API and bounded bus contexts; not CPU instruction ownership, encoder samples or complete recording acceptance'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log',type=Path)
    parser.add_argument('--fresh',action='store_true',help='require publication/read/reset reconciliation')
    args = parser.parse_args()
    try:
        print(json.dumps(check(args.log.read_text(),args.fresh),indent=2))
    except (ValueError,KeyError,OSError,ArithmeticError) as error:
        parser.exit(1,f'MU4 recorder read census FAIL: {error}\n')


if __name__ == '__main__':
    main()
