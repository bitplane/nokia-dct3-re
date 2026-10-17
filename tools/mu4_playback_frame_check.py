"""Check passive MU4 input material and pre-copy block evidence, not audio acceptance."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import sys

if __package__:
    from .mu4_context_contract import contract, reference_copy_adapter
else:
    from mu4_context_contract import contract, reference_copy_adapter

AAC_SHA = '3f0a2b7eafb22ede5b3e3f1bb54c83f2f959386bedd897ebebf218e6fb816ff4'
PCM_SHA = '440e479020b5e558b9aa792adc14af8bb4974f30898cba57f3189ac1434f8993'


def rows(text, marker):
    result = []
    for line in text.splitlines():
        if marker+':' not in line:
            continue
        fields = {}
        for token in line.split(marker+':',1)[1].split():
            key,separator,value = token.partition('=')
            if not separator or key in fields:
                raise ValueError(f'malformed {marker}')
            fields[key] = value
        if fields.get('observer_writes') != '0':
            raise ValueError('observation scope changed')
        result.append(fields)
    return result


def compare_blocks(blocks, reference):
    """Pair chronological 1024-word source blocks without fitting lag or channels."""
    if len(reference) != len(blocks)*4096 or not blocks:
        raise ValueError('reference/block extent mismatch')
    expected = struct.unpack('<'+'h'*(len(reference)//2),reference)
    reports = []
    for frame,pair in enumerate(blocks):
        if len(pair) != 2 or any(len(payload)!=2048 for payload in pair):
            raise ValueError('expected two 1024-word source blocks')
        channels = []
        for channel,payload in enumerate(pair):
            actual = struct.unpack('>1024h',payload)
            wanted = expected[frame*2048+channel:(frame+1)*2048:2]
            errors = [a-b for a,b in zip(actual,wanted)]
            first = next((i for i,error in enumerate(errors) if error),None)
            channels.append({'mismatched_samples':sum(error!=0 for error in errors),
                             'error_rms':math.sqrt(sum(error*error for error in errors)/1024),
                             'first_mismatch':None if first is None else {'sample':first,'native':actual[first],'reference':wanted[first]},
                             'native_nonzero':sum(value!=0 for value in actual),
                             'reference_nonzero':sum(value!=0 for value in wanted)})
        reports.append({'source_block_ordinal':frame,'channels':channels})
    return reports


def verify(directory, source, aac, pcm):
    source_contract = contract(source)['copy_adapter']
    if hashlib.sha256(aac).hexdigest()!=AAC_SHA or hashlib.sha256(pcm).hexdigest()!=PCM_SHA:
        raise ValueError('expected independently decoded captured eight-packet reference')
    text = (directory/'error.log').read_text()
    terminal = rows(text,'mu4_frame_terminal')
    snapshots = rows(text,'mu4_frame_snapshot')
    copies = rows(text,'mu4_frame_copy')
    reads = rows(text,'mu4_frame_bit_read')
    if len(terminal)!=1 or terminal[0].get('closed')!='1' or terminal[0].get('capped')!='0' or terminal[0].get('audio_acceptance')!='0' or not 2 <= float(terminal[0]['t']) < 2.1:
        raise ValueError('missing complete uncapped frame observation')
    end = terminal[0]
    parent = rows(text,'mu4_playback_parent_terminal')
    if len(parent)!=1 or parent[0].get('illegal')!='0' or parent[0].get('overlay_verified')!='1' or parent[0].get('opcode_mismatch')!='0' or parent[0].get('identity')!='Execute_entry' or parent[0].get('t')!=end['t']:
        raise ValueError('missing legal source-verified native execution boundary')
    if int(end['reads'])!=len(reads) or int(end['copies'])!=len(copies) or int(end['snapshots'])!=1 or int(end['outputs'])!=len(snapshots)-1:
        raise ValueError('frame observation counters disagree')
    if not 0 < len(reads) <= 4096 or not 256 <= len(copies) < 512:
        raise ValueError('insufficient or capped bus/copy evidence')
    for ordinal,row in enumerate(reads,1):
        if int(row['ordinal'])!=ordinal or row.get('ownership')!='data_bus' or row.get('mask')!='ffff' or not 0x162e <= int(row['address'],16) <= 0x1e2d or not 0x2510c <= int(row['context_pc'],16) <= 0x25143 or not 0.99 <= float(row['t']) <= 1.25:
            raise ValueError('bit-reader selection or ordinal changed')
    captured = {}
    for row in snapshots:
        name = row['file']
        if not re.fullmatch(r'mu4_frame_(initial_buffer|output_[0-9]{2})\.be16',name) or name in captured or row.get('context_only')!='1':
            raise ValueError('invalid or duplicate snapshot')
        payload = (directory/name).read_bytes()
        if len(payload)!=2*int(row['words']):
            raise ValueError('snapshot extent mismatch')
        captured[name] = (row,payload)
    initial,payload = captured['mu4_frame_initial_buffer.be16']
    if initial['base']!='162e' or len(payload)!=4096 or payload[512:512+len(aac)]!=aac:
        raise ValueError('initial decoder buffer does not contain the captured AAC bytes at the evidenced offset')
    output_rows = {(row['t'],int(row['base'],16)):(name,data)
                   for name,(row,data) in captured.items() if name!='mu4_frame_initial_buffer.be16'}
    objects,used = {},set()
    for ordinal,row in enumerate(copies,1):
        fields = [int(value,16) for value in row['fields'].split(',')]
        if len(fields)!=14 or any(not 0 <= value <= 0xffff for value in fields) or fields[2:6]!=[0,44100,0,44100] or fields[7]!=1024 or fields[11]!=64 or row['mask']!='ffff' or row['context_pc']!='02a367' or row['pointer_ownership']!='unvalidated' or int(row['ordinal'])!=ordinal or not 1 <= float(row['t']) <= 1.25:
            raise ValueError('copy observation domain changed')
        count = int(row['count'],16)
        old_source,old_destination = fields[8]-count,fields[12]-count
        model = reference_copy_adapter(fields[7],old_source,fields[11],old_destination)
        if model['count']!=count or count!=64 or int(row['address'],16)!=((fields[10]+old_destination)&0xffff):
            raise ValueError('copy count/old-cursor address does not follow source contract')
        if (model['set_source_exhausted'] and fields[9]!=1) or (model['set_destination_full'] and fields[13]!=1):
            raise ValueError('copy completion flag changed')
        object_id = row['object']
        state = objects.setdefault(object_id,{'cursor':1024,'blocks':[],'completed':0,'source':None,'snapshots':[]})
        if old_source==0:
            key = row['t'],fields[6]
            if state['cursor']!=1024 or key not in output_rows:
                raise ValueError('source rollover or matching snapshot is absent')
            name,block = output_rows[key]
            used.add(name)
            state['blocks'].append(block)
            state['snapshots'].append({'file':name,'base':fields[6],'time':row['t'],'sha256':hashlib.sha256(block).hexdigest()})
            state['source'] = fields[6]
        elif state['cursor']!=old_source or state['source']!=fields[6]:
            raise ValueError('source block order changed')
        if len(state['blocks'][-1])!=2048 or int.from_bytes(state['blocks'][-1][2*old_source:2*old_source+2],'big')!=int(row['word'],16):
            raise ValueError('source snapshot differs from observed first copied word')
        state['cursor'] = fields[8]
        if fields[8]==1024:
            state['completed'] += 1
    if len(objects)!=2 or used!={name for name in captured if name!='mu4_frame_initial_buffer.be16'} or min(state['completed'] for state in objects.values())<8:
        raise ValueError('two complete chronological source-block streams were not observed')
    ordered = list(objects.values())
    comparison = compare_blocks([tuple(state['blocks'][i] for state in ordered) for i in range(8)],pcm)
    return {'input_material_matches':len(aac),'input_offset_bytes':512,
            'copy_source_contract_words':source_contract['pinned_words'],
            'copies':len(copies),'actual_bit_reads':len(reads),
            'object_order':list(objects),'complete_blocks':[state['completed'] for state in ordered],
            'source_blocks':[state['snapshots'] for state in ordered],
            'comparison':comparison,'packet_to_output_call_binding_validated':False,
            'native_audio_validated':False,
            'scope':'Exact input material and sampled pre-copy block chronology, not all copied words, decoder call-to-packet ownership, analog audio or speech'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--source-a00',type=Path,required=True)
    parser.add_argument('--reference-aac',type=Path,required=True)
    parser.add_argument('--reference-pcm',type=Path,required=True)
    args = parser.parse_args()
    try:
        report = verify(args.directory,args.source_a00.read_bytes(),args.reference_aac.read_bytes(),args.reference_pcm.read_bytes())
    except (OSError,ValueError,KeyError) as error:
        print(f'MU4 frame observation: {error}',file=sys.stderr)
        return 1
    print(json.dumps(report,indent=2))
    return 0


if __name__=='__main__':
    raise SystemExit(main())
