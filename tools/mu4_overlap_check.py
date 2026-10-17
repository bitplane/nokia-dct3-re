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
TAIL_BLOCK=(0x24a7e,122,'b623073fc0f5f77b064d6c02cae8529808e5dd0ab7340b7470fa5321eef49d82')


def final_tail(raw,history,saved,mode):
    if len(raw)!=1024 or len(history)!=512 or len(saved)!=128 or mode not in TARGETS:
        raise ValueError('unsupported final-tail geometry')
    raw,history,saved=list(raw),list(history),list(saved)
    if mode==2:
        new_history=[((value+0x8000)>>16)&0xffff for value in raw[576:]]+history[448:]
        old=lambda word:(word-0x10000 if word&0x8000 else word)<<16
        return [old(word) for word in history[:448]]+raw[:576],new_history,raw[:128]
    if mode==3:
        new_saved=raw[896:]
        shifted=raw[512:896]+saved[:64]
        raw[576:]=reversed(shifted)
        raw[448:576]=new_saved
        saved=new_saved
    return raw,history,saved


def history_continuity(directory,text):
    events=[]
    for marker,read_stage,output_stage in (('mu4_history_snapshot','1','3'),('mu4_tail_snapshot','1','4')):
        for row in rows(text,marker):
            if row['stage'] not in (read_stage,output_stage):
                continue
            read=row['stage']==read_stage
            events.append((float(row['t']),0 if read else 1,row,read))
    state={}
    known=initial=0
    differences=[]
    for _,_,row,read in sorted(events,key=lambda item:item[:2]):
        blob=(directory/row['file']).read_bytes()
        values=struct.unpack(f'>{len(blob)//2}H',blob)
        base=int(row['base'],16)
        for offset,value in enumerate(values):
            address=base+offset
            if read:
                if address in state:
                    known+=1
                    if state[address]!=value:
                        differences.append({'address':f'{address:04x}','expected':state[address],'actual':value,'t':row['t']})
                else:
                    initial+=1
            else:
                state[address]=value
    return {'known_reads':known,'initial_reads':initial,'mismatched_words':len(differences),
            'first_differences':differences[:8],'history_continuity_validated':known>0 and not differences,
            'scope':'Observed callback/tail read boundaries, not absence of intervening writers or initial provisioning acceptance'}


def conversion_input_binding(directory,text):
    values=rows(text,'mu4_conversion_value')
    if len(values) not in (4096,16384):
        raise ValueError('conversion input binding requires two or eight complete blocks')
    blocks=len(values)//2048
    vectors={i:struct.unpack('>1024i',(directory/f'mu4_tail_{i:03d}_stage3.be16').read_bytes()) for i in range(1,2*blocks+1)}
    mismatches=0
    for ordinal,row in enumerate(values):
        frame,within=divmod(ordinal,2048)
        sample,channel=divmod(within,2)
        raw=int(row['raw'],16)
        if ((int(row['call']),int(row['sample']),int(row['channel']))!=(frame+1,sample,channel) or
                row.get('identity')!='Execute_entry' or not 0<=raw<(1<<40) or
                raw>>32 not in (0,255) or (raw>>32==255 and not raw&0x80000000)):
            raise ValueError('conversion input identity or DLD extension changed')
        # DLD may zero- or sign-extend the signed 32-bit payload. The existing
        # converter checker separately validates its signed interpretation.
        mismatches+=(raw&0xffffffff)!=(vectors[2*frame+channel+1][sample]&0xffffffff)
    return {'components':len(values),'blocks':blocks,'mismatched_components':mismatches,
            'conversion_input_binding_validated':mismatches==0,
            'scope':f'{blocks} paired final-tail blocks versus converter payloads; not a complete writer census or other media'}


def verify_tail(directory,source):
    report=verify(directory,source)
    words=source_contract(source)
    pin_block(words,*TAIL_BLOCK)
    text=(directory/'error.log').read_text()
    arguments=rows(text,'mu4_tail_arguments')
    snapshots=rows(text,'mu4_tail_snapshot')
    calls=sorted(set(row['call'] for row in report['callbacks']))
    if len(arguments)!=2*len(calls) or len(snapshots)!=6*len(arguments) or [int(r['call']) for r in arguments]!=[call for call in calls for _ in range(2)]:
        raise ValueError('expected two complete final tails per processing call')
    result=[]
    windows=rows(text,'mu4_window_snapshot')
    returned=rows(text,'mu4_processing_return')
    for ordinal,arg in enumerate(arguments,1):
        invocation=int(arg['invocation'])
        if not 1<=invocation<=len(report['callbacks']):
            raise ValueError('final tail references unknown transform')
        mode=int(arg['mode'],16)
        selected=snapshots[6*(ordinal-1):6*ordinal]
        owner=report['callbacks'][invocation-1]
        previous=float(next(r for r in windows if int(r['invocation'])==invocation and r['stage']=='2')['t'])
        limit=float(next(r for r in returned if r['call']==arg['call'])['t'])
        if (int(arg['tail'])!=ordinal or int(arg['call'])!=owner['call'] or mode!=owner['mode'] or
                arg.get('identity')!='Execute_entry' or arg.get('context_only')!='1' or arg.get('audio_acceptance')!='0' or
                [int(r['stage']) for r in selected]!=list(range(6)) or arg['t']!=selected[0]['t']):
            raise ValueError('final-tail argument identity changed')
        data=[]
        for stage,row in enumerate(selected):
            kind=stage%3
            base=int(arg[('base','history','saved')[kind]],16)
            count=(2048,512,256)[kind]
            address=0x24a7e if stage<3 else 0x24af3
            name=f'mu4_tail_{ordinal:03d}_stage{stage}.be16'
            time=float(row['t'])
            if (int(row['invocation'])!=ordinal or row['call']!=arg['call'] or
                    int(row['parameter'],16)!=1024 or int(row['address'],16)!=address or
                    int(row['opcode'],16)!=words[address] or int(row['base'],16)!=base or
                    not 0x60<=base<=0x10000-count or int(row['words'])!=count or row['file']!=name or
                    row.get('identity')!='Execute_entry' or row.get('context_only')!='1' or row.get('audio_acceptance')!='0' or
                    not previous<=time<limit):
                raise ValueError('final-tail snapshot identity, bounds or chronology changed')
            previous=time
            blob=(directory/name).read_bytes()
            if len(blob)!=count*2 or fnv(blob)!=int(row['hash'],16):
                raise ValueError('final-tail snapshot length/hash changed')
            data.append(struct.unpack(f'>{count}H',blob) if kind==1 else struct.unpack(f'>{count//2}i',blob))
        expected=final_tail(*data[:3],mode)
        bad=[sum(a!=b for a,b in zip(actual,wanted)) for actual,wanted in zip(data[3:],expected)]
        result.append({'tail':ordinal,'invocation':invocation,'call':int(arg['call']),'mode':mode,
                       'raw_mismatches':bad[0],'history_mismatches':bad[1],'saved_mismatches':bad[2]})
    continuity=history_continuity(directory,text)
    report.update({'tails':result,'final_tails_validated':not any(r['raw_mismatches'] or r['history_mismatches'] or r['saved_mismatches'] for r in result),
                   'continuity':continuity,'source_words_pinned':report['source_words_pinned']+TAIL_BLOCK[1],
                   'scope':'Captured transform/window/callback/tail arithmetic and observed history continuity; second-callback memory effects, all writer coverage, spectral correctness, PCM conformance, analog and speech remain unvalidated'})
    if 'mu4_conversion_value:' in text:
        report['conversion_input_binding']=conversion_input_binding(directory,text)
    return report


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
    parser.add_argument('--tail',action='store_true',help='also require final buffer-tail arithmetic and observed history continuity')
    args=parser.parse_args()
    try:
        result=(verify_tail if args.tail else verify)(args.directory,args.source_a00.read_bytes())
        print(json.dumps(result,indent=2))
        keys=('window_products_validated','transform_chain_validated','scaling_validated','history_exchange_validated')
        return 0 if all(result[key] for key in keys) and result.get('final_tails_validated',True) and result.get('continuity',{}).get('history_continuity_validated',True) and result.get('conversion_input_binding',{}).get('conversion_input_binding_validated',True) else 1
    except (ValueError,KeyError,OSError) as error:
        print(f'FAIL: {error}',file=sys.stderr)
        return 1


if __name__=='__main__':
    sys.exit(main())
