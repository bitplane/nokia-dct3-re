"""Reconcile actual DRR read owners; CPU PC is context, not instruction ownership."""
import argparse
from decimal import Decimal
import hashlib
import json
from pathlib import Path
import sys

if __package__:
    from .mu4_playback_frame_check import rows
    from .noki5510_bootstrap_check import verify_recorder_input
    from .noki5510_a00_inventory import uploaded_word_image
    from .mu4_clock_contract import SOURCE_SHA256
else:
    from mu4_playback_frame_check import rows
    from noki5510_bootstrap_check import verify_recorder_input
    from noki5510_a00_inventory import uploaded_word_image
    from mu4_clock_contract import SOURCE_SHA256


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


def check_destinations(text,owners):
    ends = rows(text,'mu4_recorder_destination_terminal')
    if len(ends) != 1 or ends[0] != dict(bad='0',pending='0',save_restore='unsupported',
            observer_writes='0',encoder_consumption='0'):
        raise ValueError('incomplete or inconsistent DMA destination pairing')
    expected = {channel:sum(count for mask,count in owners.items() if mask&(1<<channel))
                for channel in range(6)}
    observed = dict.fromkeys(range(6),0)
    destinations,seen = [],set()
    for row in rows(text,'mu4_recorder_destination'):
        channel,space,count = (int(row[key]) for key in ('channel','space','writes'))
        low,high = (int(row[key],16) for key in ('minimum','maximum'))
        if (not 0 <= channel < 6 or not 0 <= space < 3 or (channel,space) in seen
                or count <= 0 or not 0 <= low <= high <= (0x7fffff if space == 0 else 0xffff)
                or row['observer_writes'] != '0'):
            raise ValueError('invalid DMA destination histogram')
        seen.add((channel,space)); observed[channel] += count
        destinations.append(dict(channel=channel,space=('program','data','io')[space],
                                 writes=count,minimum=f'{low:06x}',maximum=f'{high:06x}'))
    if observed != expected or not destinations:
        raise ValueError('DMA source reads and destination writes do not reconcile')
    return dict(destinations=destinations,paired_values_validated=True,
                scope='Bus destination ranges only; not contiguous coverage, persistent buffer ownership or encoder consumption')


def check_buffers(text,destinations):
    ends = rows(text,'mu4_recorder_buffer_terminal')
    if len(ends) != 1 or ends[0] != dict(bad='0',dropped='0',context_cap='256',
            context_pc='not_instruction_owner',save_restore='unsupported',observer_writes='0',encoder_consumption='0'):
        raise ValueError('incomplete or inconsistent recorder buffer observation')
    if destinations is None:
        raise ValueError('buffer observation requires DMA destination pairing')
    expected = [0,0]
    for row in destinations['destinations']:
        low,high = int(row['minimum'],16),int(row['maximum'],16)
        bank = (low-0x1800)//256
        if row['space'] != 'data' or bank not in (0,1) or not 0x1800+bank*256 <= low <= high < 0x1900+bank*256:
            raise ValueError('DMA destinations escape observed recorder buffers')
        expected[bank] += row['writes']
    summaries,readers,contexts = {},{},{}
    for row in rows(text,'mu4_recorder_buffer'):
        bank = int(row['bank'])
        writes,epochs,retired,pending,unknown = (int(row[key]) for key in
            ('writes','read_epochs','retired_unread','pending_unread','unknown_reads'))
        if (bank not in (0,1) or bank in summaries or min(writes,epochs,retired,pending,unknown) < 0
                or pending > 256 or writes != expected[bank] or writes != epochs+retired+pending
                or row['observer_writes'] != '0'):
            raise ValueError('recorder buffer write lifetimes do not reconcile')
        summaries[bank] = dict(writes=writes,read_epochs=epochs,retired_unread=retired,
                               pending_unread=pending,unknown_reads=unknown)
    for row in rows(text,'mu4_recorder_buffer_reader'):
        bank,mask,count = int(row['bank']),int(row['dma_mask'],16),int(row['reads'])
        if bank not in (0,1) or not 0 <= mask < 64 or count <= 0 or (bank,mask) in readers or row['observer_writes'] != '0':
            raise ValueError('invalid buffer reader histogram')
        readers[bank,mask] = count
    for row in rows(text,'mu4_recorder_buffer_context'):
        bank,pc,count = int(row['bank']),int(row['context_pc'],16),int(row['reads'])
        if bank not in (0,1) or not 0 <= pc <= 0x7fffff or count <= 0 or (bank,pc) in contexts or row['observer_writes'] != '0':
            raise ValueError('invalid buffer read context histogram')
        contexts[bank,pc] = count
    if set(summaries) != {0,1} or len(contexts) > 256:
        raise ValueError('missing or capped buffer lifetime census')
    for bank,summary in summaries.items():
        count = sum(value for (owner,_),value in readers.items() if owner == bank)
        if count < summary['read_epochs'] or sum(value for (owner,_),value in contexts.items() if owner == bank) != readers.get((bank,0),0):
            raise ValueError('buffer read epochs/contexts do not reconcile')
        summary['known_reads'] = count
        summary['readers'] = {f'{mask:02x}':value for (owner,mask),value in readers.items() if owner == bank}
        summary['unowned_contexts'] = {f'{pc:06x}':value for (owner,pc),value in contexts.items() if owner == bank}
    return dict(banks=summaries,scope='Observed bus read lifetimes, not instruction ownership, copy semantics or encoder consumption')


def check_read_identity(text,buffers):
    ends = rows(text,'mu4_recorder_buffer_identity_terminal')
    if len(ends) != 1 or ends[0] != dict(missing='0',dropped='0',cap='256',
            identity='staged_operand_bus_read',save_restore='unsupported',observer_writes='0',retirement='0'):
        raise ValueError('missing or capped staged operand read identity')
    if buffers is None:
        raise ValueError('instruction identity requires buffer lifetime census')
    seen,counts,result = set(),[0,0],[]
    for row in rows(text,'mu4_recorder_buffer_instruction'):
        bank,address,opcode,count = int(row['bank']),int(row['instruction'],16),int(row['opcode'],16),int(row['reads'])
        key = bank,address,opcode
        if (bank not in (0,1) or not 0 <= address <= 0x7fffff or not 0 <= opcode <= 0xffff
                or count <= 0 or key in seen or row['observer_writes'] != '0'):
            raise ValueError('invalid staged read instruction histogram')
        seen.add(key); counts[bank] += count
        result.append(dict(bank=bank,instruction=f'{address:06x}',opcode=f'{opcode:04x}',reads=count))
    if len(seen) > 256 or counts != [buffers['banks'][bank]['readers'].get('00',0) for bank in range(2)]:
        raise ValueError('staged read identities do not cover non-DMA buffer reads')
    return dict(instructions=result,scope='Captured staged operand bus-read instruction/opcode; not retirement or encoder consumption')


def pin_read_instructions(identities,words):
    if identities is None:
        raise ValueError('source pinning requires staged read identities')
    result = []
    for row in identities['instructions']:
        address,opcode = int(row['instruction'],16),int(row['opcode'],16)
        if words.get(address) != opcode:
            raise ValueError('captured read opcode does not match original AA88 source')
        result.append(dict(row,operation='MVDD Xmem,Ymem' if opcode&0xff00 == 0xe500 else 'unclassified'))
    return dict(segment='aa88',instructions=result,
                scope='Original source opcode matches actual staged bus-read identity; not copy destination, full routine identity or encoder consumption')


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
    destinations = check_destinations(text,owners) if rows(text,'mu4_recorder_destination_terminal') else None
    buffers = check_buffers(text,destinations) if rows(text,'mu4_recorder_buffer_terminal') else None
    identities = check_read_identity(text,buffers) if rows(text,'mu4_recorder_buffer_identity_terminal') else None
    return {'reads':total,'stop_seconds':str(stop),'drr_freshness':freshness,'dma_destinations':destinations,'buffer_lifetimes':buffers,
            'read_instructions':identities,
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
    parser.add_argument('--destinations',action='store_true',help='require exact DMA read/write pairing')
    parser.add_argument('--buffers',action='store_true',help='require receive-buffer write/read lifetimes')
    parser.add_argument('--instructions',action='store_true',help='require staged operand read identities')
    parser.add_argument('--source-a00',type=Path,help='pin captured read opcodes to original R060 AA88 upload')
    args = parser.parse_args()
    try:
        result = check(args.log.read_text(),args.fresh)
        if args.destinations and result['dma_destinations'] is None:
            raise ValueError('DMA destination observation required')
        if args.buffers and result['buffer_lifetimes'] is None:
            raise ValueError('recorder buffer lifetime observation required')
        if args.instructions and result['read_instructions'] is None:
            raise ValueError('staged buffer read instruction observation required')
        if args.source_a00:
            source = args.source_a00.read_bytes()
            if hashlib.sha256(source).hexdigest() != SOURCE_SHA256:
                raise ValueError('expected original R060 InitData source')
            words = uploaded_word_image(bytes.fromhex(source.decode('ascii')),'aa88')
            result['read_source'] = pin_read_instructions(result['read_instructions'],words)
        print(json.dumps(result,indent=2))
    except (ValueError,KeyError,OSError,ArithmeticError) as error:
        parser.exit(1,f'MU4 recorder read census FAIL: {error}\n')


if __name__ == '__main__':
    main()
