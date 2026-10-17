"""Check captured native window-product helper I/O, not full overlap or audio."""
import argparse
import json
from pathlib import Path
import struct
import sys

if __package__:
    from .mu4_transform_check import source_contract, pin_block, partial_product, wrap32, fnv, verify_pre
    from .mu4_playback_frame_check import rows
else:
    from mu4_transform_check import source_contract, pin_block, partial_product, wrap32, fnv, verify_pre
    from mu4_playback_frame_check import rows

BLOCKS=((0x25bb3,103,'d4b1fedd10478b4b334a6294acb55777a849c33590d5fa25f9426b78aa507ae0'),
        (0x24a64,15,'116b550c5ec3ca7399b59f9a992dfc01c3e518199633ad763b121824d7e08280'))


def window_products(values,coefficients):
    if len(values) not in (128,1024) or len(coefficients)!=len(values):
        raise ValueError('window helper requires complete short/long operands')
    count=len(values)//2
    out=list(values)
    for i in range(count):
        left,right=values[count-1-i],values[count+i]
        c,d=coefficients[i],coefficients[-1-i]
        out[count-1-i]=wrap32(partial_product(left,d)-partial_product(right,c))
        out[count+i]=wrap32(partial_product(right,d)+partial_product(left,c))
    return out[:count][::-1]+out[count:][::-1]


def verify(directory,source):
    words=source_contract(source)
    for block in BLOCKS:
        pin_block(words,*block)
    transforms=verify_pre(directory,source)
    text=(directory/'error.log').read_text()
    invocations=rows(text,'mu4_fft_invocation')
    snapshots=rows(text,'mu4_window_snapshot')
    posts=rows(text,'mu4_post_snapshot')
    returns=rows(text,'mu4_processing_return')
    if not invocations or len(snapshots)!=3*len(invocations):
        raise ValueError('expected complete window snapshots for every transform')
    reports=[]
    for ordinal,entry in enumerate(invocations,1):
        selected=snapshots[3*(ordinal-1):3*ordinal]
        if len(selected)!=3:
            raise ValueError('window helper snapshots incomplete')
        parameter=int(selected[0]['parameter'],16)
        if parameter not in (128,1024):
            raise ValueError('unsupported observed window geometry')
        if [int(r['stage']) for r in selected]!=[0,1,2]:
            raise ValueError('window helper stages missing or duplicated')
        data={}
        previous=float(next(r for r in posts if int(r['invocation'])==ordinal and r['stage']=='2')['t'])
        returned=float(next(r for r in returns if r['call']==entry['call'])['t'])
        for stage,row in enumerate(selected):
            address=0x24a72 if stage==2 else 0x24a70
            base=int(row['base'],16)
            name=f'mu4_window_{ordinal:03d}_stage{stage}.be16'
            time=float(row['t'])
            if (row['call']!=entry['call'] or int(row['invocation'])!=ordinal or
                    int(row['parameter'],16)!=parameter or int(row['words'])!=2*parameter or
                    int(row['address'],16)!=address or int(row['opcode'],16)!=words[address] or
                    not 0x60<=base<=0x10000-2*parameter or row['file']!=name or
                    row.get('identity')!='Execute_entry' or row.get('context_only')!='1' or
                    row.get('audio_acceptance')!='0' or not previous<=time<returned):
                raise ValueError('window helper identity, bounds or chronology changed')
            if stage==2 and row['base']!=selected[0]['base']:
                raise ValueError('window helper output base changed')
            previous=time
            blob=(directory/name).read_bytes()
            if len(blob)!=4*parameter or fnv(blob)!=int(row['hash'],16):
                raise ValueError('window helper snapshot length/hash mismatch')
            data[stage]=struct.unpack(f'>{parameter}i',blob)
        if ordinal<len(invocations) and previous>float(invocations[ordinal]['t']):
            raise ValueError('window helper overlaps next transform')
        expected=window_products(data[0],data[1])
        differences=[{'component':i,'native':a,'expected':b}
                     for i,(a,b) in enumerate(zip(data[2],expected)) if a!=b]
        reports.append({'invocation':ordinal,'call':int(entry['call']),'components':parameter,
                        'fft_parameter':entry['parameter'],
                        'base':selected[0]['base'],'coefficient_base':selected[1]['base'],
                        'mismatched_components':len(differences),'first_differences':differences[:8]})
    return {'windows':reports,'window_products_validated':not any(r['mismatched_components'] for r in reports),
            'transform_chain_validated':all(transforms[k] for k in ('pre_transform_validated','selected_transform_validated','post_transform_validated')),
            'source_words_pinned':transforms['source_words_pinned']+sum(n for _,n,_ in BLOCKS),
            'native_audio_validated':False,
            'scope':'Captured window-product helper only; scale/dispatch, overlap carry, final buffer moves, spectral correctness, PCM conformance, analog and speech remain unvalidated'}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--source-a00',type=Path,required=True)
    args=parser.parse_args()
    try:
        result=verify(args.directory,args.source_a00.read_bytes())
        print(json.dumps(result,indent=2))
        return 0 if result['window_products_validated'] and result['transform_chain_validated'] else 1
    except (ValueError,KeyError,OSError) as error:
        print(f'FAIL: {error}',file=sys.stderr)
        return 1


if __name__=='__main__':
    sys.exit(main())
