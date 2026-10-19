"""Reconcile actual DRR read owners; CPU PC is context, not instruction ownership."""
import argparse
from decimal import Decimal
import hashlib
import json
from pathlib import Path
import sys

INGRESS_BASE = 0x39817
INGRESS_PROLOGUE_BASE = 0x397f9
INGRESS_PROLOGUE = (0x4a11,0x4a16,0x4a17,0x6bf8,0x0018,0xf7fd,0xf495,0xf495,
                    0xe787,0x8811,0xf495,0x71e7,0x0808,0x0016,0x1081,0x80e7,
                    0x0802,0x10f8,0xef96,0xf845,0x9817,0x71e1,0x0002,0xef92,
                    0x1086,0xf983,0x9945,0x76f8,0xef96,0x0000)
INGRESS_WORDS = (0x10e7,0x0802,0xf010,0x0001,0x8813,0xe782,0x6dea,0x0002,
                 0x771a,0x03ff,0xf072,0x9830,0x10eb,0x0001,0x8082,0x60e1,
                 0x0002,0x0002,0xf820,0x982f,0x10e3,0x0400,0x80e2,0x0001,
                 0x6dea,0x0002,0x1186,0x30f8,0xef92,0x20e1,0x0001,0x8001,
                 0x4818,0x8100,0xf000,0x0002,0xf983,0x9852)

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
        operation = ('MVDD Xmem,Ymem' if opcode&0xff00 == 0xe500 else
                     f'LD Smem,{"B" if opcode&0x100 else "A"}' if opcode&0xfe00 == 0x1000 else 'unclassified')
        result.append(dict(row,operation=operation))
    return dict(segment='aa88',instructions=result,
                scope='Original source opcode matches actual staged bus-read identity; not copy destination, full routine identity or encoder consumption')


def check_copies(text,identities):
    ends = rows(text,'mu4_recorder_copy_terminal')
    if len(ends) != 1 or identities is None:
        raise ValueError('copy pairing requires read identity and one terminal')
    end = ends[0]
    for key,value in dict(pending='0',bad='0',dropped='0',pending_cap='64',site_cap='256',
            identity='staged_read_write',save_restore='unsupported',observer_writes='0',encoder_consumption='0').items():
        if end[key] != value:
            raise ValueError('incomplete or inconsistent staged copy pairing')
    expected = {(row['bank'],int(row['instruction'],16),int(row['opcode'],16)):row['reads']
                for row in identities['instructions'] if int(row['opcode'],16)&0xff00 == 0xe500}
    seen,known,unknown,result = set(),{},0,[]
    for row in rows(text,'mu4_recorder_copy_destination'):
        bank,address,opcode = int(row['bank']),int(row['instruction'],16),int(row['opcode'],16)
        key = bank,address,opcode
        new,old = int(row['known_writes']),int(row['unknown_writes'])
        low,high,dest_low,dest_high = (int(row[field],16) for field in
            ('source_min','source_max','destination_min','destination_max'))
        if (bank not in (0,1) or not 0 <= address <= 0x7fffff or not 0 <= opcode <= 0xffff
                or opcode&0xff00 != 0xe500 or key in seen or min(new,old) < 0 or new+old <= 0
                or not 0x1800+bank*256 <= low <= high < 0x1900+bank*256
                or not 0 <= dest_low <= dest_high <= 0xffff or row['observer_writes'] != '0'):
            raise ValueError('invalid copy destination histogram')
        seen.add(key); unknown += old
        if new:
            known[key] = new
        result.append(dict(bank=bank,instruction=f'{address:06x}',opcode=f'{opcode:04x}',
                           known_writes=new,unknown_writes=old,source_min=f'{low:04x}',
                           source_max=f'{high:04x}',destination_min=f'{dest_low:04x}',destination_max=f'{dest_high:04x}'))
    if (len(seen) > 256 or known != expected or sum(known.values()) != int(end['known_reads'])
            or unknown != int(end['unknown_reads']) or sum(known.values())+unknown != int(end['writes'])):
        raise ValueError('copy destination writes do not reconcile with operand reads')
    return dict(destinations=result,paired_words=int(end['writes']),
                scope='Instruction/source/value paired actual MVDD bus writes; ranges are not contiguous coverage or encoder consumption')


def check_workspace(text,copies):
    ends = rows(text,'mu4_recorder_workspace_terminal')
    if len(ends) != 1 or copies is None:
        raise ValueError('workspace provenance requires paired copy writes and one terminal')
    end = ends[0]
    for key,value in dict(bad='0',dropped='0',cap='256',save_restore='unsupported',observer_writes='0',encoder_consumption='0').items():
        if end[key] != value:
            raise ValueError('incomplete or inconsistent workspace provenance')
    names = ('origin_writes','total_writes','alias_writes','read_epochs','retired_unread',
             'pending_unread','known_reads','unknown_reads','missing_data_identity')
    counts = {key:int(end[key]) for key in names}
    expected = 0
    for row in copies['destinations']:
        low,high = int(row['destination_min'],16),int(row['destination_max'],16)
        if low <= 0x7fff and high >= 0x7000:
            if low < 0x7000 or high > 0x7fff:
                raise ValueError('copy range crosses workspace observation boundary')
            expected += row['known_writes']
    if (min(counts.values()) < 0 or counts['origin_writes'] != expected
            or counts['total_writes'] < expected or counts['alias_writes'] > counts['total_writes']
            or counts['pending_unread'] > 4096 or counts['read_epochs'] > counts['known_reads']
            or expected != counts['read_epochs']+counts['retired_unread']+counts['pending_unread']):
        raise ValueError('workspace write lifetimes do not reconcile')
    readers,seen,instructions = {},set(),[]
    for row in rows(text,'mu4_recorder_workspace_reader'):
        alias,mask,count = int(row['program_alias']),int(row['dma_mask'],16),int(row['reads'])
        if alias not in (0,1) or not 0 <= mask < 64 or count <= 0 or (alias,mask) in readers or row['observer_writes'] != '0':
            raise ValueError('invalid workspace read owner histogram')
        readers[alias,mask] = count
    for row in rows(text,'mu4_recorder_workspace_instruction'):
        address,opcode,count = int(row['instruction'],16),int(row['opcode'],16),int(row['reads'])
        if not 0 <= address <= 0x7fffff or not 0 <= opcode <= 0xffff or count <= 0 or (address,opcode) in seen or row['observer_writes'] != '0':
            raise ValueError('invalid workspace instruction histogram')
        seen.add((address,opcode)); instructions.append(dict(instruction=f'{address:06x}',opcode=f'{opcode:04x}',reads=count))
    if (len(seen) > 256 or sum(readers.values()) != counts['known_reads']
            or sum(row['reads'] for row in instructions)+counts['missing_data_identity'] != readers.get((0,0),0)):
        raise ValueError('workspace readers/identities do not reconcile')
    return dict(counts,readers=[dict(program_alias=alias,dma_mask=f'{mask:02x}',reads=count)
                               for (alias,mask),count in sorted(readers.items())],instructions=instructions,
                scope='Latest copied-word bus read lifetimes with shared program/data invalidation; not forwarded operands, derived values or encoder consumption')


def decode_ingress_loop(words):
    if any(words.get(base+index) != word for base,witness in
           ((INGRESS_PROLOGUE_BASE,INGRESS_PROLOGUE),(INGRESS_BASE,INGRESS_WORDS)) for index,word in enumerate(witness)) or words.get(0x3983d) != 0xf7b8:
        raise ValueError('original AA88 sample-repack loop changed or absent')
    return dict(first=f'{INGRESS_PROLOGUE_BASE:06x}',pinned_words=len(INGRESS_PROLOGUE)+len(INGRESS_WORDS)+1,iterations=1024,
                stereo_source_separation=1024,destination_stride=2,
                stack_reserve_words=2051,buffer_offset=2,buffer_words=2048,callee_argument='A low word = SP + 2',
                load_sites=['039823','03982b'],store_sites=['039825','03982d'],
                call='03983b',callee='039852',return_site='03983d',scope='Selected stereo repack arm, not complete callee or encoder-output semantics')


def check_ingress(text,workspace):
    ends = rows(text,'mu4_recorder_ingress_terminal')
    if len(ends) != 1 or workspace is None:
        raise ValueError('sample repack requires workspace provenance and one terminal')
    end = ends[0]
    for key,value in dict(pending_reads='0',pending_write='0',pending_call='0',partial='0,0',
            bad='0',dropped='0',frame_cap='512',queue_cap='4',save_restore='unsupported',observer_writes='0',encoder_consumption='0').items():
        if end[key] != value:
            raise ValueError('incomplete or inconsistent sample repack observation')
    count = int(end['frames'])
    def pair(row,key,base=10):
        values = tuple(int(value,base) for value in row[key].split(','))
        if len(values) != 2 or min(values) < 0:
            raise ValueError('invalid sample repack pair')
        return values
    totals = [count*1024]*2
    observed = {row['instruction']:row for row in workspace['instructions']}
    expected = []
    for instruction,opcode in (('039823','10eb'),('03982b','10e3')):
        row = observed.get(instruction)
        if row is None or row['opcode'] != opcode:
            raise ValueError('sample repack source identities absent')
        expected.append(row['reads'])
    if (not 0 < count <= 512 or int(end['entries']) != count or expected != totals
            or sum(expected) != workspace['known_reads'] or pair(end,'loads') != tuple(totals)
            or pair(end,'stores') != tuple(totals) or pair(end,'known') != tuple(totals)):
        raise ValueError('sample repack loops do not cover known workspace reads')
    frames = rows(text,'mu4_recorder_ingress_frame')
    sources,destinations = {},{}
    if len(frames) != count:
        raise ValueError('missing or repeated sample repack frame')
    for ordinal,row in enumerate(frames,1):
        source,destination = pair(row,'source_base',16),pair(row,'destination_base',16)
        if (int(row['ordinal']) != ordinal or pair(row,'stores') != (1024,1024)
                or pair(row,'known') != (1024,1024) or row['identity'] != 'Execute_entry' or row['observer_writes'] != '0'
                or not all(0x7000 <= value <= 0x7fff for value in source)
                or source[1] != 0x7000+((source[0]-0x7000+1024)&0xfff)
                or max(destination) > 0xffff or destination[1] != (destination[0]+1)&0xffff
                or int(row['argument'],16) != destination[0]):
            raise ValueError('sample repack frame geometry changed or incomplete')
        for histogram,values in ((sources,source),(destinations,destination)):
            key = ','.join(f'{value:04x}' for value in values)
            histogram[key] = histogram.get(key,0)+1
    return dict(frames=count,samples_per_lane=1024,paired_stores=sum(totals),
                source_bases=sources,destination_bases=destinations,call='03983b',callee='039852',
                scope='Actual known-word loads, equal-value stores, stereo geometry and next callee entry; not encoder output or speech')


def check_consumer(text,ingress):
    ends = rows(text,'mu4_recorder_consumer_terminal')
    if len(ends) != 1 or ingress is None:
        raise ValueError('stack consumer requires validated repack ABI and one terminal')
    end = ends[0]
    for key,value in dict(bad='0',dropped='0',active='0',cap='256',frame_cap='512',
            save_restore='unsupported',observer_writes='0',encoder_consumption='0').items():
        if end[key] != value:
            raise ValueError('incomplete or inconsistent stack consumer observation')
    names = ('origins','known_reads','read_epochs','retired_unread','unread_return',
             'unknown_reads','writes','alias_writes','missing_data_identity')
    totals = {key:int(end[key]) for key in names}
    frames = rows(text,'mu4_recorder_consumer_frame')
    inputs = rows(text,'mu4_recorder_ingress_frame')
    count = ingress['frames']
    if (int(end['entries']) != count or int(end['returns']) != count or len(frames) != count
            or len(inputs) != count or totals['origins'] != ingress['paired_stores'] or min(totals.values()) < 0):
        raise ValueError('stack consumer lifetimes do not match repack calls')
    sums = dict.fromkeys(names,0)
    for ordinal,(row,source) in enumerate(zip(frames,inputs),1):
        values = {key:int(row[key]) for key in names}
        if (int(row['ordinal']) != ordinal or int(source['ordinal']) != ordinal
                or int(row['base'],16) != int(source['argument'],16) or row['observer_writes'] != '0'
                or min(values.values()) < 0 or values['origins'] != 2048
                or values['origins'] != values['read_epochs']+values['retired_unread']+values['unread_return']
                or values['known_reads'] < values['read_epochs'] or values['writes'] < values['retired_unread']
                or values['alias_writes'] > values['writes'] or values['missing_data_identity'] > values['known_reads']):
            raise ValueError('invalid stack sample lifetime frame')
        for key,value in values.items():
            sums[key] += value
    if sums != totals:
        raise ValueError('stack sample lifetime totals do not reconcile')
    readers,seen,instructions = {},set(),[]
    for row in rows(text,'mu4_recorder_consumer_reader'):
        alias,mask,count = int(row['program_alias']),int(row['dma_mask'],16),int(row['reads'])
        if alias not in (0,1) or not 0 <= mask < 64 or count <= 0 or (alias,mask) in readers or row['observer_writes'] != '0':
            raise ValueError('invalid stack sample reader histogram')
        readers[alias,mask] = count
    for row in rows(text,'mu4_recorder_consumer_instruction'):
        address,opcode,count = int(row['instruction'],16),int(row['opcode'],16),int(row['reads'])
        if not 0 <= address <= 0x7fffff or not 0 <= opcode <= 0xffff or count <= 0 or (address,opcode) in seen or row['observer_writes'] != '0':
            raise ValueError('invalid stack sample instruction histogram')
        seen.add((address,opcode)); instructions.append(dict(instruction=f'{address:06x}',opcode=f'{opcode:04x}',reads=count))
    if (len(seen) > 256 or sum(readers.values()) != totals['known_reads']
            or sum(row['reads'] for row in instructions)+totals['missing_data_identity'] != readers.get((0,0),0)):
        raise ValueError('stack sample readers/identities do not reconcile')
    return dict(totals,frames=ingress['frames'],readers=[dict(program_alias=alias,dma_mask=f'{mask:02x}',reads=count)
                                                     for (alias,mask),count in sorted(readers.items())],instructions=instructions,
                scope='Original stack sample bus reads within each callee lifetime; not forwarded operands, post-write derived values or complete encoder output')


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
    copies = check_copies(text,identities) if rows(text,'mu4_recorder_copy_terminal') else None
    workspace = check_workspace(text,copies) if rows(text,'mu4_recorder_workspace_terminal') else None
    ingress = check_ingress(text,workspace) if rows(text,'mu4_recorder_ingress_terminal') else None
    consumer = check_consumer(text,ingress) if rows(text,'mu4_recorder_consumer_terminal') else None
    return {'reads':total,'stop_seconds':str(stop),'drr_freshness':freshness,'dma_destinations':destinations,'buffer_lifetimes':buffers,
            'read_instructions':identities,
            'copy_destinations':copies,
            'copied_workspace':workspace,
            'sample_repack':ingress,
            'stack_consumer':consumer,
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
    parser.add_argument('--copies',action='store_true',help='require instruction/source/value paired MVDD writes')
    parser.add_argument('--workspace',action='store_true',help='require alias-aware copied-workspace read lifetimes')
    parser.add_argument('--ingress',action='store_true',help='require sample-repack load/store and callee-entry closure')
    parser.add_argument('--consumer',action='store_true',help='require per-call stack sample read lifetimes')
    args = parser.parse_args()
    try:
        result = check(args.log.read_text(),args.fresh)
        if args.destinations and result['dma_destinations'] is None:
            raise ValueError('DMA destination observation required')
        if args.buffers and result['buffer_lifetimes'] is None:
            raise ValueError('recorder buffer lifetime observation required')
        if args.instructions and result['read_instructions'] is None:
            raise ValueError('staged buffer read instruction observation required')
        if args.copies and result['copy_destinations'] is None:
            raise ValueError('MVDD read/write pairing required')
        if args.workspace and result['copied_workspace'] is None:
            raise ValueError('copied-workspace observation required')
        if args.ingress and result['sample_repack'] is None:
            raise ValueError('sample-repack observation required')
        if args.consumer and result['stack_consumer'] is None:
            raise ValueError('stack sample consumer observation required')
        if args.source_a00:
            source = args.source_a00.read_bytes()
            if hashlib.sha256(source).hexdigest() != SOURCE_SHA256:
                raise ValueError('expected original R060 InitData source')
            words = uploaded_word_image(bytes.fromhex(source.decode('ascii')),'aa88')
            result['read_source'] = pin_read_instructions(result['read_instructions'],words)
            if result['copied_workspace'] is not None:
                result['workspace_source'] = pin_read_instructions(result['copied_workspace'],words)
            if result['sample_repack'] is not None:
                result['sample_repack_source'] = decode_ingress_loop(words)
            if result['stack_consumer'] is not None:
                candidates = [dict(row,aa88_word=f'{words[int(row["instruction"],16)]:04x}' if int(row['instruction'],16) in words else None,
                                   opcode_matches=words.get(int(row['instruction'],16)) == int(row['opcode'],16))
                              for row in result['stack_consumer']['instructions']]
                result['stack_consumer_source_candidates'] = dict(instructions=candidates,
                    scope='AA88 source-word comparisons only, not full routine or overlay identity')
        print(json.dumps(result,indent=2))
    except (ValueError,KeyError,OSError,ArithmeticError) as error:
        parser.exit(1,f'MU4 recorder read census FAIL: {error}\n')


if __name__ == '__main__':
    main()
