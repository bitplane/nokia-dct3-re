"""Check bounded original 64-complex transform stages, not decoded audio."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

if __package__:
    from .mu4_clock_contract import SOURCE_SHA256
    from .mu4_playback_frame_check import rows
    from .noki5510_a00_inventory import uploaded_word_image
else:
    from mu4_clock_contract import SOURCE_SHA256
    from mu4_playback_frame_check import rows
    from noki5510_a00_inventory import uploaded_word_image


BLOCKS = ((0x248fd,88,'fa9f2a488a08609c99f5d36fc503479e9dbd22f369328ea1ac96ac02411b0207'),
          (0x23fc5,41,'229439d4be1a9172efecf797502886199462b5690f724be11539706b9d4a7d3d'),
          (0x23fee,217,'b3dc83b9b2677259bf4d0ff832f66ea43efdcffaf6eb564d4bb8128ea3acce3f'),
          (0x240c7,69,'1e1bd04a2418d6e2bc7cab0de04045c9b831b341a7f2837f9e2167ce51662f41'),
          (0x2410c,145,'3b9fcca3c3b4fdbfd4252e27209d03279192df085bf607e121ac0136b62ae3d5'))
BLOCKS += ((0x2419d,85,'5d370d4c811616cfa8f212069536645325982ee85040f3fc9f87171ee9af56ba'),)
SITES = {0:0x24919,1:0x2400a,2:0x2401e,3:0x24036,4:0x24051,5:0x2406c,6:0x2491e,7:0x24919,8:0x24919}


def pin_block(words,base,count,digest):
    try:
        data=b''.join(struct.pack('>H',words[base+i]) for i in range(count))
    except KeyError as error:
        raise ValueError('transform source block incomplete') from error
    if hashlib.sha256(data).hexdigest()!=digest:
        raise ValueError('transform source block changed')


def source_contract(source):
    if hashlib.sha256(source).hexdigest()!=SOURCE_SHA256:
        raise ValueError('expected original R060 source')
    words=uploaded_word_image(bytes.fromhex(source.decode('ascii')),'aa99')
    for block in BLOCKS:
        pin_block(words,*block)
    return words


def wrap32(value):
    return (value+(1<<31))%(1<<32)-(1<<31)


def radix4(values):
    if not values or len(values)%8:
        raise ValueError('radix-4 stage requires complete four-complex groups')
    out=[]
    for i in range(0,len(values),8):
        r0,i0,r1,i1,r2,i2,r3,i3=values[i:i+8]
        rd=(r0-r1)>>2
        idiff=i2-i3
        r23=(r2-r3)>>2
        isum=(i2+i3)>>2
        out.extend(((r0+r1+r2+r3)>>2,(i0+i1+4*isum)>>2,
                    rd-(idiff>>2),(i0-i1+4*r23)>>2,
                    (r0+r1-r2-r3)>>2,(i0+i1-4*isum)>>2,
                    rd-(idiff>>2)+(idiff>>1),(i0-i1-4*r23)>>2))
    return list(map(wrap32,out))


def partial_product(value,coefficient):
    coefficient=wrap32(coefficient)
    high,low=value>>16,value&0xffff
    chi,clo=coefficient>>16,coefficient&0xffff
    return (((clo*high+low*chi)>>16)+high*chi)<<3


def stage3(values,coefficient):
    if not values or len(values)%16:
        raise ValueError('stage 3 requires complete eight-complex groups')
    out=list(values)
    for base in range(0,len(values),16):
        for j in range(4):
            xR,xI=values[base+2*j:base+2*j+2]
            qR,qI=values[base+8+2*j:base+10+2*j]
            if j==0:
                real,imag=qR,qI
            elif j==2:
                real,imag=-qI,qR
            else:
                a,b=partial_product(qR,coefficient),partial_product(qI,coefficient)
                real,imag=(a-b,a+b) if j==1 else (-a-b,a-b)
            out[base+2*j]=wrap32((xR+real)>>1)
            out[base+2*j+1]=wrap32((xI+imag)>>1)
            out[base+8+2*j]=wrap32((xR-real)>>1)
            out[base+9+2*j]=wrap32((xI-imag)>>1)
    return out


def weighted_stage(values,coefficients,width):
    if width not in (16,32,64) or not values or len(values)%(2*width) or len(coefficients)!=512:
        raise ValueError('weighted stage requires complete supported groups and coefficient table')
    def parts(value,coefficient):
        return ((coefficient&0xffff)*(value>>16)+(value&0xffff)*(coefficient>>16),
                (value>>16)*(coefficient>>16))
    out=list(values)
    for base in range(0,len(values),2*width):
        for j in range(width//2):
            at,other=base+2*j,base+width+2*j
            xr,xi=values[at:at+2]
            qr,qi=values[other:other+2]
            if j==0:
                real,imag=qr,qi
            else:
                index=512*j//width-1
                cr,ci=coefficients[index],coefficients[index+128]
                a,b=parts(qr,cr),parts(qi,ci)
                # The real cross terms are combined before the single shift;
                # separately truncating both products changes low bits.
                real=(((a[0]+b[0])>>16)+a[1]+b[1])<<3
                a,b=parts(qi,cr),parts(qr,ci)
                imag=(((a[0]>>16)+a[1])-((b[0]>>16)+b[1]))<<3
            out[at]=wrap32((xr+real)>>1)
            out[at+1]=wrap32((xi+imag)>>1)
            out[other]=wrap32((xr-real)>>1)
            out[other+1]=wrap32((xi-imag)>>1)
    return out


def fnv(data):
    result=2166136261
    for byte in data:
        result=((result^byte)*16777619)&0xffffffff
    return result


def invocation_inventory(text, words):
    """Enumerate observed wrapper calls, without claiming vector correctness."""
    entries=rows(text,'mu4_processing_entry')
    returns=rows(text,'mu4_processing_return')
    invocations=rows(text,'mu4_fft_invocation')
    if ([r['call'] for r in entries]!=list(map(str,range(1,9))) or
            [r['call'] for r in returns]!=list(map(str,range(1,9))) or not invocations):
        raise ValueError('transform inventory requires eight complete processing calls')
    result=[]
    previous=-1.0
    for call,(entry,returned) in enumerate(zip(entries,returns),1):
        selected=[r for r in invocations if r.get('call')==str(call)]
        if not selected:
            raise ValueError('processing call lacks observed transform invocations')
        counts={}
        for row in selected:
            address=int(row['address'],16)
            parameter=int(row['parameter'],16)
            base=int(row['base'],16)
            time=float(row['t'])
            if (address not in (0x24919,0x24944) or
                    parameter!=(0x80 if address==0x24919 else 0x400) or
                    int(row['opcode'],16)!=words[address] or
                    row.get('identity')!='Execute_entry' or row.get('context_only')!='1' or
                    row.get('observer_writes')!='0' or row.get('audio_acceptance')!='0' or
                    not 0x60<=base<=0x10000-2*parameter or
                    not float(entry['t'])<time<float(returned['t']) or time<previous):
                raise ValueError('transform invocation identity, bounds or chronology changed')
            previous=time
            key=(parameter,base)
            counts[key]=counts.get(key,0)+1
        result.append({'call':call,'invocations':len(selected),
                       'vectors':[{'parameter':f'{p:04x}','complex_points':p//2,
                                   'base':f'{b:04x}','count':n}
                                  for (p,b),n in sorted(counts.items())]})
    if sum(r['invocations'] for r in result)!=len(invocations):
        raise ValueError('transform inventory includes unknown processing call')
    return {'processing_calls':result,'invocations':len(invocations),
            'native_audio_validated':False,
            'scope':'Observed wrapper entries and geometry only; no additional vector arithmetic acceptance'}


def verify(directory,source):
    words=source_contract(source)
    text=(directory/'error.log').read_text()
    snapshots=rows(text,'mu4_fft_snapshot')
    if len(snapshots)!=18:
        raise ValueError('expected nine complete snapshots per selected channel')
    entry=[r for r in rows(text,'mu4_processing_entry') if r['call']=='2']
    returned=[r for r in rows(text,'mu4_processing_return') if r['call']=='2']
    if len(entry)!=1 or len(returned)!=1:
        raise ValueError('transform snapshots lack enclosing packet-bound processing call')
    reports=[]
    for channel in (0,1):
        selected=[r for r in snapshots if int(r['channel'])==channel]
        if [int(r['stage']) for r in selected]!=[0,7,8,1,2,3,4,5,6]:
            raise ValueError('transform stage sequence missing or duplicated')
        data={}
        for row in selected:
            stage=int(row['stage'])
            base=(0xf400 if stage==7 else 0x117a if stage==8 else (0x7800,0xe800)[channel])
            count=1024 if stage==7 else 2 if stage==8 else 256
            name=f'mu4_fft_ch{channel}_stage{stage}.be16'
            if (row.get('identity')!='Execute_entry' or row.get('context_only')!='1' or
                    int(row['address'],16)!=SITES[stage] or int(row['opcode'],16)!=words[SITES[stage]] or
                    int(row['parameter'],16)!=128 or int(row['base'],16)!=base or int(row['words'])!=count or row['file']!=name):
                raise ValueError('transform snapshot identity/geometry changed')
            if not float(entry[0]['t'])<float(row['t'])<float(returned[0]['t']):
                raise ValueError('transform snapshot outside selected processing call')
            blob=(directory/name).read_bytes()
            if len(blob)!=count*2 or fnv(blob)!=int(row['hash'],16):
                raise ValueError('transform snapshot length/hash mismatch')
            data[stage]=blob
        if any(float(a['t'])>float(b['t']) for a,b in zip(selected,selected[1:])):
            raise ValueError('transform snapshots not chronological')
        if data[5]!=data[6]:
            raise ValueError('transform return changes final stage vector')
        vectors={stage:list(struct.unpack('>128i',data[stage])) for stage in range(7)}
        coefficient=int.from_bytes(data[8],'big')
        if coefficient!=(words[0x117a]<<16)|words[0x117b]:
            raise ValueError('stage-3 coefficient differs from original upload')
        if radix4(vectors[0])!=vectors[1]:
            raise ValueError('complete initial radix-4 stage differs from source arithmetic')
        expected=stage3(vectors[1],coefficient)
        differences=[{'component':i,'native':a,'expected':b} for i,(a,b) in enumerate(zip(vectors[2],expected)) if a!=b]
        coefficients=struct.unpack('>512i',data[7])
        later=[]
        for stage,width in ((3,16),(4,32),(5,64)):
            expected_stage=weighted_stage(vectors[stage-1],coefficients,width)
            mismatches=[{'component':i,'native':a,'expected':b} for i,(a,b) in enumerate(zip(vectors[stage],expected_stage)) if a!=b]
            later.append({'stage':stage+1,'mismatched_components':len(mismatches),'first_differences':mismatches[:8]})
        reports.append({'channel':channel,'complex_points':64,'initial_radix4_validated':True,
                        'stage3_mismatched_components':len(differences),'first_stage3_differences':differences[:8],
                        'stage3_validated':not differences,'coefficient':f'{coefficient:08x}',
                        'later_stages':later,'selected_transform_validated':not differences and not any(r['mismatched_components'] for r in later)})
    if (directory/'mu4_fft_ch0_stage7.be16').read_bytes()!=(directory/'mu4_fft_ch1_stage7.be16').read_bytes():
        raise ValueError('selected channels use different retained coefficient tables')
    return {'channels':reports,'source_words_pinned':sum(n for _,n,_ in BLOCKS),
            'native_audio_validated':False,'selected_transform_validated':all(r['selected_transform_validated'] for r in reports),
            'scope':'Two selected 64-complex arithmetic transforms using captured coefficients; no all-frame, long-transform, input correctness, PCM, analog or speech acceptance'}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--source-a00',type=Path,required=True)
    parser.add_argument('--inventory',action='store_true',help='report observed short/long wrapper entries, not arithmetic acceptance')
    args=parser.parse_args()
    try:
        source=args.source_a00.read_bytes()
        if args.inventory:
            result=invocation_inventory((args.directory/'error.log').read_text(),source_contract(source))
            print(json.dumps(result,indent=2))
            return 0
        result=verify(args.directory,source)
        print(json.dumps(result,indent=2))
        return 0 if result['selected_transform_validated'] else 1
    except (ValueError,KeyError,OSError) as error:
        print(f'FAIL: {error}',file=sys.stderr)
        return 1


if __name__=='__main__':
    sys.exit(main())
