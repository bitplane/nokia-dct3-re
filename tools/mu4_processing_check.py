"""Verify bounded native packet consumption and conversion, not decoded audio quality."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

if __package__:
    from .mu4_playback_frame_check import rows, verify as verify_frames
    from .mu4_context_contract import contract, reference_pcm_conversion
    from .mu4_recorder_media_inventory import adts_header
else:
    from mu4_playback_frame_check import rows, verify as verify_frames
    from mu4_context_contract import contract, reference_pcm_conversion
    from mu4_recorder_media_inventory import adts_header


def signed40(value):
    if not 0 <= value < 1 << 40:
        raise ValueError('accumulator is outside its 40-bit domain')
    return value-(1<<40) if value & (1<<39) else value


def match_packets(entries, returns, reads, aac):
    """Cover each consumed byte using entry reservoir and actual in-call bus reads."""
    if len(entries)!=len(returns) or not entries:
        raise ValueError('incomplete processing call pairs')
    offset,proofs = 0,[]
    for ordinal,(entry,returned) in enumerate(zip(entries,returns),1):
        header = adts_header(aac,offset)
        if header is None or not header['frame_within_file'] or int(entry['call'])!=ordinal or int(returned['call'])!=ordinal:
            raise ValueError('packet/call ordering mismatch')
        remaining,end_remaining = int(entry['remaining'],16),int(returned['remaining'],16)
        if remaining not in (0,8,16,24,32) or end_remaining not in (0,8,16,24,32):
            raise ValueError('packet boundary is not byte-aligned in the reservoir')
        start = int(entry['index'],16)*2-remaining//8
        end = int(returned['index'],16)*2-end_remaining//8
        length = header['frame_bytes']
        if end-start!=length or float(entry['t'])>=float(returned['t']):
            raise ValueError('net bit-reader consumption differs from declared packet length')
        covered = set()
        reservoir = int(entry['reservoir'],16).to_bytes(4,'big')
        prefix = reservoir[4-remaining//8:]
        if prefix!=aac[offset:offset+len(prefix)]:
            raise ValueError('entry reservoir differs from reference packet')
        covered.update(range(len(prefix)))
        actual_reads = unvalidated_lookahead = 0
        for read in reads:
            if not float(entry['t'])<=float(read['t'])<=float(returned['t']):
                continue
            actual_reads += 1
            relative = (int(read['address'],16)-int(entry['base'],16))*2-start
            for i,value in enumerate(int(read['word'],16).to_bytes(2,'big')):
                at = relative+i
                if at < 0 or at >= length+end_remaining//8:
                    raise ValueError('bit-reader access is outside packet plus retained lookahead')
                position = offset+at
                if position < len(aac):
                    if value!=aac[position]:
                        raise ValueError('actual packet read differs from independent reference')
                else:
                    unvalidated_lookahead += 1
                if at < length:
                    covered.add(at)
        if len(covered)!=length or not actual_reads:
            raise ValueError('consumed packet bytes lack complete bus/reservoir coverage')
        proofs.append({'call':ordinal,'offset':offset,'bytes':length,'covered_bytes':len(covered),
                       'actual_reads':actual_reads,'unvalidated_unconsumed_lookahead_bytes':unvalidated_lookahead})
        offset += length
    if offset!=len(aac):
        raise ValueError('reference contains packets outside processing observation')
    return proofs


def verify(directory, source, aac, pcm, *, require_eight_blocks=False):
    material = verify_frames(directory,source,aac,pcm)
    source_contract = contract(source)['pcm_conversion']
    text = (directory/'error.log').read_text()
    entries,returns,blocks,values = [rows(text,name) for name in
        ('mu4_processing_entry','mu4_processing_return','mu4_conversion_block','mu4_conversion_value')]
    if [len(entries),len(returns),len(blocks)]!=[8,8,8] or len(values) not in (4096,16384):
        raise ValueError('missing or duplicated bounded processing/conversion records')
    conversion_blocks=len(values)//2048
    if require_eight_blocks and conversion_blocks!=8:
        raise ValueError('eight complete conversion blocks required')
    if any(row.get('identity')!='Execute_entry' for row in entries+returns+blocks+values) or any(row.get('context_only')!='1' for row in entries):
        raise ValueError('processing observation identity changed')
    packets = match_packets(entries,returns,rows(text,'mu4_frame_bit_read'),aac)
    for frame,(entry,returned,block) in enumerate(zip(entries,returns,blocks)):
        caller = int(entry['caller_sp'],16)
        if ((int(entry['sp'],16)+2)&0xffff)!=caller or int(returned['sp'],16)!=caller or int(entry['a'],16)!=((caller+6)&0xffff) or not (int(entry['st1'],16)&0x4000):
            raise ValueError('processing ABI/call-reserved stack observation changed')
        if int(returned['result'],16)!=0 or int(block['call'])!=frame+1 or int(block['frame'])!=frame or block['bad']!='0' or block['samples']!='1024':
            raise ValueError('processing did not complete the checked conversion loop')
        if not float(entry['t'])<float(returned['t'])<float(block['t']):
            raise ValueError('processing/conversion time order changed')
        if frame and float(blocks[frame-1]['t'])>=float(entry['t']):
            raise ValueError('processing calls overlap the previous conversion')
        destination = int(returned['destination'],16)
        if any(not 0x60 <= int(returned[name],16) <= 0xf800 for name in ('input0','input1')):
            raise ValueError('raw sample source crosses the checked RAM domain')
        if int(block['destination'],16)!=destination:
            raise ValueError('conversion destination changed')
        for channel in (0,1):
            snapshot = material['source_blocks'][channel][frame]
            if snapshot['base']!=destination+channel*1024 or float(snapshot['time'])<=float(block['t']):
                raise ValueError('conversion block is not bound to the later copy snapshot')
            if frame+2<len(entries) and float(snapshot['time'])>=float(entries[frame+2]['t']):
                raise ValueError('snapshot follows the next processing call that can reuse its destination')
    captured = [[struct.unpack('>1024H',(directory/material['source_blocks'][c][f]['file']).read_bytes())
                 for c in (0,1)] for f in range(conversion_blocks)]
    first_differences = []
    reference_errors=[]
    reference = struct.unpack('<16384h',pcm)
    for ordinal,row in enumerate(values):
        frame,within = divmod(ordinal,2048)
        sample,channel = divmod(within,2)
        if (int(row['call']),int(row['frame']),int(row['sample']),int(row['channel']),int(row['argument']))!=(frame+1,frame,sample,channel,14) or row['retirement']!='unclaimed':
            raise ValueError('conversion record order or argument changed')
        expected = reference_pcm_conversion(int(row['raw'],16))
        result = signed40(int(row['result'],16))
        word = result&0xffff
        if signed40(int(row['shifted'],16))!=expected['shifted'] or result!=max(-32767,min(32767,expected['shifted'])) or word!=expected['word']:
            raise ValueError('native conversion differs from independent rounded-shift/clamp')
        if not float(returns[frame]['t'])<float(row['t'])<float(blocks[frame]['t']):
            raise ValueError('conversion value is outside its processing return/output window')
        destination = int(returns[frame]['destination'],16)+sample+1024*channel
        if int(row['destination'],16)!=destination or captured[frame][channel][sample]!=word:
            raise ValueError('converted result differs from later source RAM')
        wanted = reference[2048*frame+2*sample+channel]
        actual = word-65536 if word&0x8000 else word
        reference_errors.append(actual-wanted)
        if actual!=wanted and len(first_differences)<4:
            source_base = int(returns[frame]['input0' if channel==0 else 'input1'],16)
            first_differences.append({'frame':frame,'sample':sample,'channel':channel,
                                      'raw_source_word_address':f'{source_base+2*sample:04x}',
                                      'raw':row['raw'],'native':actual,'reference':wanted})
    return {'packets':packets,'processing_calls':8,'conversion_words':len(values),
            'conversion_blocks':conversion_blocks,
            'conversion_source_words':source_contract['pinned_words'],
            'rounded_shift_mismatches':0,'clamp_mismatches':0,'stored_word_mismatches':0,
            'first_reference_differences':first_differences,
            'reference_comparison_diagnostic':{'samples':len(reference_errors),
                'mismatched_samples':sum(error!=0 for error in reference_errors),
                'max_absolute_error':max(map(abs,reference_errors)),
                'rms_error':math.sqrt(sum(error*error for error in reference_errors)/len(reference_errors)),
                'fitted_alignment':False,'codec_tolerance_established':False},
            'packet_to_output_call_binding_validated':True,'native_audio_validated':False,
            'scope':f'Eight consumed packets and output loops; {conversion_blocks} blocks have complete conversion/store correspondence. Final three lookahead bytes are unconsumed, not validated reference content. No decoded-waveform, analog or speech acceptance'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--source-a00',type=Path,required=True)
    parser.add_argument('--reference-aac',type=Path,required=True)
    parser.add_argument('--reference-pcm',type=Path,required=True)
    parser.add_argument('--require-eight-blocks',action='store_true',help='reject legacy two-block conversion observations')
    args = parser.parse_args()
    try:
        result = verify(args.directory,args.source_a00.read_bytes(),args.reference_aac.read_bytes(),args.reference_pcm.read_bytes(),require_eight_blocks=args.require_eight_blocks)
    except (OSError,ValueError,KeyError) as error:
        print(f'MU4 processing observation: {error}',file=sys.stderr)
        return 1
    print(json.dumps(result,indent=2))
    return 0


if __name__=='__main__':
    raise SystemExit(main())
