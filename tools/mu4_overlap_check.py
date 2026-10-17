"""Check selected window callbacks and buffer exchange, not complete audio."""
import argparse
import json
from pathlib import Path
import struct
import sys

if __package__:
    from .mu4_window_check import verify_scale
    from .mu4_transform_check import source_contract, pin_block, fnv
    from .mu4_playback_frame_check import rows
else:
    from mu4_window_check import verify_scale
    from mu4_transform_check import source_contract, pin_block, fnv
    from mu4_playback_frame_check import rows

BLOCKS=((0x24af8,159,'008371856a79251fc343a7ef31c82e97aa346a10c0271335e7aa829eedfe4024'),
        (0x24955,63,'d381e531b115ee40f2838b53013543cb1076bcffbfe3196a45912628fc93c299'),
        (0x249c5,75,'79d88f3fd08c1e29734ede62471b0dda44f4a8d42662479ac6922a1f22ec3078'),
        (0x24a5c,8,'940df4725a90ccfbfad5176f74381aeb128a09d1b3dee361f8ed4f2fe68df9aa'))
TARGETS={0:(0x24af8,0x24955),1:(0x24b13,0x24955),2:(0x24b3e,0x2496e),3:(0x24b59,0x2496e)}


def exchange(values,history,mode):
    count=64 if mode==2 else 512
    if mode not in TARGETS or len(history)!=count or len(values)!=2*count:
        raise ValueError('unsupported callback buffer geometry')
    if any(not 0<=word<=0xffff for word in history):
        raise ValueError('history must contain 16-bit words')
    signed=lambda word:word-0x10000 if word&0x8000 else word
    out=list(values)
    saved=[((value+0x8000)>>16)&0xffff for value in values[:count]]
    if mode==3:
        out[896:960]=[signed(word)<<16 for word in history[-1:-65:-1]]
        out[:count]=[signed(word)<<16 for word in history]
    else:
        out[:count]=[signed(word)<<16 for word in reversed(history)]
    return out,saved[::-1]


def verify(directory,source):
    report=verify_scale(directory,source)
    words=source_contract(source)
    for block in BLOCKS:
        pin_block(words,*block)
    text=(directory/'error.log').read_text()
    callbacks=rows(text,'mu4_window_callback')
    snapshots=rows(text,'mu4_history_snapshot')
    scales=rows(text,'mu4_scale_snapshot')
    windows=rows(text,'mu4_window_snapshot')
    count=len(report['windows'])
    if len(callbacks)!=6*count or len(snapshots)!=4*count:
        raise ValueError('callback calls/entries/returns or buffer snapshots incomplete')
    result=[]
    for ordinal in range(1,count+1):
        selected=callbacks[6*(ordinal-1):6*ordinal]
        mode=int(selected[0]['mode'],16)
        if mode not in TARGETS:
            raise ValueError('unmapped window callback mode')
        previous=float(next(r for r in scales if int(r['invocation'])==ordinal and r['stage']=='1')['t'])
        limit=float(next(r for r in windows if int(r['invocation'])==ordinal and r['stage']=='0')['t'])
        call=str(report['windows'][ordinal-1]['call'])
        parameter=int(selected[0]['parameter'],16)
        for i,row in enumerate(selected):
            site,phase=divmod(i,3)
            target=TARGETS[mode][site]
            address=(0x24a60 if site==0 else 0x24a63) if phase==0 else target if phase==1 else (0x24a61 if site==0 else 0x24a64)
            time=float(row['t'])
            if (int(row['invocation'])!=ordinal or row['call']!=call or int(row['site'])!=site or
                    row['phase']!=('call','entry','return')[phase] or int(row['target'],16)!=target or
                    int(row['mode'],16)!=mode or int(row['parameter'],16)!=parameter or
                    row['base']!=selected[3*site]['base'] or row['history']!=selected[3*site]['history'] or
                    int(row['address'],16)!=address or int(row['opcode'],16)!=words[address] or
                    row.get('identity')!='Execute_entry' or row.get('context_only')!='1' or
                    row.get('audio_acceptance')!='0' or not previous<=time<limit):
                raise ValueError('callback dispatch, entry or return changed')
            previous=time
        data=[]
        chosen=snapshots[4*(ordinal-1):4*ordinal]
        if [int(r['stage']) for r in chosen]!=[0,1,2,3]:
            raise ValueError('history snapshot stage order changed')
        for stage,row in enumerate(chosen):
            first=selected[0] if stage<2 else selected[2]
            base=int(first['history' if stage%2 else 'base'],16)
            size=(64 if mode==2 else 512) if stage%2 else 2*parameter
            name=f'mu4_history_{ordinal:03d}_stage{stage}.be16'
            if (int(row['invocation'])!=ordinal or row['call']!=call or int(row['parameter'],16)!=parameter or
                    row['address']!=first['address'] or row['opcode']!=first['opcode'] or row['t']!=first['t'] or
                    int(row['base'],16)!=base or not 0x60<=base<=0x10000-size or int(row['words'])!=size or
                    row['file']!=name or row.get('identity')!='Execute_entry' or row.get('context_only')!='1' or row.get('audio_acceptance')!='0'):
                raise ValueError('history snapshot identity or geometry changed')
            blob=(directory/name).read_bytes()
            if len(blob)!=2*size or fnv(blob)!=int(row['hash'],16):
                raise ValueError('history snapshot length/hash changed')
            data.append(struct.unpack(f'>{size}H',blob) if stage%2 else struct.unpack(f'>{parameter}i',blob))
        expected,history=exchange(data[0],data[1],mode)
        raw_bad=sum(a!=b for a,b in zip(data[2],expected))
        history_bad=sum(a!=b for a,b in zip(data[3],history))
        result.append({'invocation':ordinal,'call':int(call),'mode':mode,'components':parameter,
                       'history_words':len(history),'raw_mismatches':raw_bad,'history_mismatches':history_bad})
    report.update({'callbacks':result,'callback_dispatch_validated':True,
                   'history_exchange_validated':not any(r['raw_mismatches'] or r['history_mismatches'] for r in result),
                   'source_words_pinned':report['source_words_pinned']+sum(n for _,n,_ in BLOCKS),
                   'scope':'Captured dispatcher targets and first-callback buffer exchange; second-callback memory effects, cross-call history ownership, final moves, spectral correctness, PCM conformance, analog and speech remain unvalidated'})
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--source-a00',type=Path,required=True)
    args=parser.parse_args()
    try:
        result=verify(args.directory,args.source_a00.read_bytes())
        print(json.dumps(result,indent=2))
        keys=('window_products_validated','transform_chain_validated','scaling_validated','history_exchange_validated')
        return 0 if all(result[key] for key in keys) else 1
    except (ValueError,KeyError,OSError) as error:
        print(f'FAIL: {error}',file=sys.stderr)
        return 1


if __name__=='__main__':
    sys.exit(main())
