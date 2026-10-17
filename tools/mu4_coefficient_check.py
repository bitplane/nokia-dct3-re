"""Check original finite-difference coefficient publication, not full audio."""
import argparse
import json
from pathlib import Path
import struct
import sys

if __package__:
    from .mu4_overlap_check import verify_tail
    from .mu4_transform_check import source_contract, pin_block, fnv
    from .mu4_playback_frame_check import rows
else:
    from mu4_overlap_check import verify_tail
    from mu4_transform_check import source_contract, pin_block, fnv
    from mu4_playback_frame_check import rows

BLOCKS=((0x25c35,117,'6edc8b8c7c01bdf037d7fa7eebcd2ccde141e7dff6714910c972ed77b0763ddc'),
        (0x25ffa,164,'f492d16b3910c5b5f3602f4cd17c3218ebeeef72a57afbde60333cca2452d8dc'))
DESCRIPTORS={0xbbe6:True,0xc4e8:True,0xbc72:False,0xc572:False}


def signed(value,bits):
    return (value+(1<<(bits-1)))%(1<<bits)-(1<<(bits-1))


def checked32(value):
    if not -(1<<31)<=value<(1<<31):
        raise ValueError('coefficient recurrence left the validated non-saturating domain')
    return value


def decode_table(words,address):
    packed=DESCRIPTORS[address]
    n,k=words[address],words[address+1]
    if n not in (128,1024) or not 0<=k<n or (packed and (n-k-1)%4):
        raise ValueError('coefficient descriptor geometry changed')
    pair=lambda at:signed((words[at]<<16)|words[at+1],32)
    prefix,corrections=words[address+8],words[address+9]
    out=[pair(prefix+2*i) for i in range(k)]
    if packed:
        first,second=signed(words[address+2],16),signed(words[address+3],16)
        third=pair(address+4)
        out.append(pair(address+6))
        for i in range(n-k-1):
            delta=signed((words[corrections+i//4]>>(12-4*(i%4)))&15,4)
            value=first+delta
            first=signed(value,16)
            value+=second
            second=signed(value,16)
            value+=third
            third=checked32(value)
            out.append(checked32(value+out[-1]))
    else:
        first,second=pair(address+2),pair(address+4)
        out.append(pair(address+6))
        for i in range(n-k-1):
            first+=signed(words[corrections+i],16)
            value=first+second
            second=checked32(value)
            out.append(checked32(value+out[-1]))
    return tuple(out)


def updates(target,flag0,flag1):
    if not all(0<=flag<=0xffff for flag in (flag0,flag1)):
        raise ValueError('coefficient flags must be 16-bit values')
    if target==0x24955:
        return [(0xf000,0xc4e8 if flag0 else 0xbbe6)]
    if target==0x2496e:
        return ([(0xf100,0xc572)] if flag0 or flag1 else [])+([(0xf000,0xbc72)] if not flag0 or not flag1 else [])
    raise ValueError('unknown coefficient callback')


def verify(directory,source):
    report=verify_tail(directory,source)
    words=source_contract(source)
    for block in BLOCKS:
        pin_block(words,*block)
    tables={address:struct.pack(f'>{len(values)}i',*values) for address in DESCRIPTORS for values in [decode_table(words,address)]}
    text=(directory/'error.log').read_text()
    selections=rows(text,'mu4_coefficient_selection')
    snapshots=rows(text,'mu4_coefficients_snapshot')
    callbacks=rows(text,'mu4_window_callback')
    windows=rows(text,'mu4_window_snapshot')
    if len(selections)!=len(report['callbacks']) or len(snapshots)!=2*len(selections):
        raise ValueError('coefficient publication observations incomplete')
    result=[]
    for ordinal,selection in enumerate(selections,1):
        call=next(r for r in callbacks if int(r['invocation'])==ordinal and r['site']=='1' and r['phase']=='call')
        returned=next(r for r in callbacks if int(r['invocation'])==ordinal and r['site']=='1' and r['phase']=='return')
        if (int(selection['invocation'])!=ordinal or any(selection[key]!=call[key] for key in ('call','target','base','t')) or
                selection.get('identity')!='Execute_entry' or selection.get('context_only')!='1' or selection.get('audio_acceptance')!='0'):
            raise ValueError('coefficient selector identity changed')
        blobs=[]
        for stage,row in enumerate(snapshots[2*(ordinal-1):2*ordinal]):
            expected=call if stage==0 else returned
            name=f'mu4_coefficients_{ordinal:03d}_stage{stage}.be16'
            if (int(row['invocation'])!=ordinal or int(row['stage'])!=stage or int(row['base'],16)!=0xf000 or
                    int(row['words'])!=2048 or row['file']!=name or
                    any(row[key]!=expected[key] for key in ('call','address','opcode','parameter','t')) or
                    row.get('identity')!='Execute_entry' or row.get('context_only')!='1' or row.get('audio_acceptance')!='0'):
                raise ValueError('coefficient cache snapshot identity changed')
            blob=(directory/name).read_bytes()
            if len(blob)!=4096 or fnv(blob)!=int(row['hash'],16):
                raise ValueError('coefficient cache snapshot length/hash changed')
            blobs.append(blob)
        expected=bytearray(blobs[0])
        chosen=updates(int(selection['target'],16),int(selection['flag0'],16),int(selection['flag1'],16))
        for destination,descriptor in chosen:
            offset=2*(destination-0xf000)
            expected[offset:offset+len(tables[descriptor])]=tables[descriptor]
        mismatches=sum(a!=b for a,b in zip(struct.unpack('>2048H',blobs[1]),struct.unpack('>2048H',expected)))
        window=next(r for r in windows if int(r['invocation'])==ordinal and r['stage']=='1')
        offset=2*(int(window['base'],16)-0xf000)
        extent=2*int(window['words'])
        if not 0<=offset<=4096-extent:
            raise ValueError('consumed coefficient region outside captured cache')
        consumed=(directory/window['file']).read_bytes()
        if blobs[1][offset:offset+extent]!=consumed:
            raise ValueError('window coefficient operands differ from published cache')
        result.append({'invocation':ordinal,'call':int(selection['call']),
                       'flag0':int(selection['flag0'],16),'flag1':int(selection['flag1'],16),
                       'updates':[{'destination':f'{d:04x}','descriptor':f'{a:04x}'} for d,a in chosen],
                       'cache_word_mismatches':mismatches})
    report.update({'coefficients':result,'coefficient_publication_validated':not any(r['cache_word_mismatches'] for r in result),
                   'source_words_pinned':report['source_words_pinned']+sum(n for _,n,_ in BLOCKS),
                   'scope':'Selected descriptor recurrences, full coefficient cache and consumed operand continuity; generator scratch effects, all writer coverage, other media, spectral correctness, PCM conformance, analog and speech remain unvalidated'})
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--source-a00',type=Path,required=True)
    args=parser.parse_args()
    try:
        result=verify(args.directory,args.source_a00.read_bytes())
        print(json.dumps(result,indent=2))
        keys=('window_products_validated','transform_chain_validated','scaling_validated','history_exchange_validated','final_tails_validated','coefficient_publication_validated')
        return 0 if all(result[key] for key in keys) and result['continuity']['history_continuity_validated'] and result.get('conversion_input_binding',{}).get('conversion_input_binding_validated',True) else 1
    except (ValueError,KeyError,OSError) as error:
        print(f'FAIL: {error}',file=sys.stderr)
        return 1


if __name__=='__main__':
    sys.exit(main())
