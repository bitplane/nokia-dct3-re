"""Validate two native partial-product prefixes; no full decoder acceptance."""
import argparse
import json
from pathlib import Path
import sys

if __package__:
    from .mu4_playback_frame_check import rows
    from .mu4_raw_output_check import verify as verify_copies
    from .mu4_context_contract import (contract, RAW_PRODUCT_BASE, RAW_PRODUCT_WITNESS, reference_raw_product,
                                       RAW_UPSTREAM_WITNESSES, reference_raw_complex_prefix, reference_raw_scale, reference_raw_butterfly)
else:
    from mu4_playback_frame_check import rows
    from mu4_raw_output_check import verify as verify_copies
    from mu4_context_contract import (contract, RAW_PRODUCT_BASE, RAW_PRODUCT_WITNESS, reference_raw_product,
                                     RAW_UPSTREAM_WITNESSES, reference_raw_complex_prefix, reference_raw_scale, reference_raw_butterfly)


def read_material(reads,begin,end,addresses):
    result={}
    for row in reads:
        address=int(row['address'],16)
        if address in addresses and begin<=float(row['t'])<=end:
            if int(row['mask'],16)!=0xffff or row.get('ownership')!='data_bus':
                raise ValueError('upstream operand bus scope changed')
            value=int(row['value'],16)
            if not 0<=value<=0xffff or (address in result and result[address]!=value):
                raise ValueError('upstream operand changed or is outside its word domain')
            result[address]=value
    if set(result)!=set(addresses):
        raise ValueError('upstream operand reads incomplete')
    return result


def verify_butterfly(text):
    executed,reads,writes=[rows(text,marker) for marker in
                           ('mu4_upstream_execute','mu4_upstream_read','mu4_raw_writer')]
    addresses=[0x2419d,0x2419f,0x241a0,0x241a1,0x241a2,0x241a3,0x241a5,0x241a6,
               0x241a7,0x241a8,0x241aa,0x241ac,0x241ad,0x241af,0x241b1,0x241b2,
               0x241b3,0x241b5,0x241b7,0x241b8,0x241ba,0x241bc]
    proofs=[]
    for base in (0x7800,0xe800):
        starts=[i for i,r in enumerate(executed) if int(r['address'],16)==0x2419d and int(r['ar2'],16)==base]
        if len(starts)!=1:
            raise ValueError('selected butterfly missing or duplicated')
        block=executed[starts[0]:starts[0]+len(addresses)]
        if [int(r['address'],16) for r in block]!=addresses:
            raise ValueError('butterfly instruction sequence incomplete')
        for row in block:
            if row.get('identity')!='Execute_entry' or row.get('context_only')!='1' or int(row['st1'],16)&0x3c0!=0x100 or int(row['opcode'],16)!=RAW_UPSTREAM_WITNESSES[0x2419d][int(row['address'],16)-0x2419d]:
                raise ValueError('butterfly instruction/status regime changed')
        other=int(block[0]['ar3'],16)
        targets=list(range(base,base+4))+list(range(other,other+4))
        begin,end=float(block[0]['t']),float(block[-1]['t'])
        material=read_material(reads,begin,end,targets)
        def pair(address):
            return (material[address]<<16)|material[address+1]
        inputs=[pair(base),pair(other),pair(base+2),pair(other+2)]
        expected,outputs=reference_raw_butterfly(*inputs)
        for row in block:
            for accumulator,value in expected.get(int(row['address'],16),{}).items():
                if int(row[accumulator],16)!=value:
                    raise ValueError('native butterfly intermediate differs from scalar arithmetic')
        stores=[r for r in writes if begin<=float(r['t'])<=end and int(r['address'],16) in targets]
        if len(stores)!=8 or any(int(r['mask'],16)!=0xffff for r in stores):
            raise ValueError('butterfly output writes incomplete')
        actual={int(r['address'],16):int(r['value'],16) for r in stores}
        for address,value in zip((base,other,base+2,other+2),outputs):
            if actual.get(address)!=value>>16 or actual.get(address+1)!=value&0xffff:
                raise ValueError('butterfly shifted output store differs')
        proofs.append({'base':f'{base:04x}','other':f'{other:04x}',
                       'inputs':[f'{x:08x}' for x in inputs],
                       'outputs':[f'{x:08x}' for x in outputs]})
    return proofs


def verify_upstream(text, products):
    executed,reads=rows(text,'mu4_upstream_execute'),rows(text,'mu4_upstream_read')
    terminal=rows(text,'mu4_upstream_terminal')
    if len(terminal)!=1 or terminal[0].get('closed')!='1' or terminal[0].get('capped')!='0' or int(terminal[0]['reads'])!=len(reads):
        raise ValueError('upstream operand capture incomplete or capped')
    if any(r.get('identity')!='Execute_entry' or r.get('context_only')!='1' for r in executed) or any(
            r.get('ownership')!='data_bus' or int(r['mask'],16)!=0xffff for r in reads):
        raise ValueError('upstream identity or bus scope changed')
    for row in executed:
        address=int(row['address'],16)
        origin=next((base for base,witness in RAW_UPSTREAM_WITNESSES.items() if base<=address<base+len(witness)),None)
        if origin is None:
            continue
        if int(row['opcode'],16)!=RAW_UPSTREAM_WITNESSES[origin][address-origin]:
            raise ValueError('upstream opcode differs from source')
    def material(begin,end,addresses):
        return read_material(reads,begin,end,addresses)
    proofs=[]
    for channel,base in enumerate((0x7880,0xe880)):
        starts=[i for i,r in enumerate(executed) if int(r['address'],16)==0x23f8a and int(r['ar2'],16)==base]
        if len(starts)!=1:
            raise ValueError('selected complex prefix missing or duplicated')
        addresses=list(range(0x23f8a,0x23f98))+[0x23f99]
        block=executed[starts[0]:starts[0]+len(addresses)]
        if [int(r['address'],16) for r in block]!=addresses or any(int(r['st1'],16)&0x3c0!=0x100 for r in block):
            raise ValueError('complex prefix sequence/status changed')
        cbase,dbase=int(block[0]['ar4'],16),int(block[0]['ar5'],16)-1
        targets=list(range(base,base+4))+[cbase,cbase+1,dbase,dbase+1]
        values=material(float(block[0]['t']),float(block[-1]['t']),targets)
        def pair(address):
            return (values[address]<<16)|values[address+1]
        x,y,c,d=pair(base),pair(base+2),pair(cbase),pair(dbase)
        expected,raw=reference_raw_complex_prefix(x,y,c,d)
        for row in block:
            for accumulator,value in expected.get(int(row['address'],16),{}).items():
                if int(row[accumulator],16)!=value:
                    raise ValueError('complex difference intermediate differs from source-derived arithmetic')
        scales=[i for i,r in enumerate(executed) if int(r['address'],16)==0x25ce2 and int(r['ar2'],16)==base]
        if len(scales)!=1:
            raise ValueError('selected scale call missing or duplicated')
        end=next((i for i in range(scales[0],len(executed)) if int(executed[i]['address'],16)==0x25d0e),None)
        if end is None:
            raise ValueError('selected scale return absent')
        scale=executed[scales[0]:end+1]
        if any(int(r['st1'],16)&0x3c0!=0x100 for r in scale):
            raise ValueError('scale arithmetic status regime changed')
        initial=int(scale[0]['a'],16)
        if initial!=(raw | (0xff00000000 if raw & 0x80000000 else 0)):
            raise ValueError('complex difference does not reach scale input unchanged')
        argument=next((r for r in scale if int(r['address'],16)==0x25cea),None)
        if argument is None:
            raise ValueError('scale argument reader missing')
        first,second=int(argument['sp'],16)+5,int(argument['sp'],16)+6
        args=material(float(scale[0]['t']),float(scale[-1]['t']),[first,second])
        delta,final=reference_raw_scale(initial,args[first],args[second])
        if delta>=0:
            raise ValueError('selected negative scale branch changed')
        shifts=[r for r in scale if int(r['address'],16)==0x25d07]
        if len(shifts)!=-delta:
            raise ValueError('scale repeat count differs from arguments')
        value=initial
        for row in shifts:
            if int(row['a'],16)!=value:
                raise ValueError('scale shift intermediate differs')
            value=(value<<1)&((1<<40)-1)
        if value!=final or int(scale[-1]['a'],16)!=final or final&0xffffffff!=int(products[channel]['input'],16):
            raise ValueError('scale output does not match native product input')
        proofs.append({'channel':channel,'inputs':[f'{x:08x}',f'{y:08x}'],
                       'coefficients':[f'{c:08x}',f'{d:08x}'],'complex_result':f'{raw:08x}',
                       'scale_arguments':[args[first],args[second]],'left_shifts':-delta,
                       'scaled_accumulator':f'{final:010x}','product_input':products[channel]['input']})
    butterflies=verify_butterfly(text)
    for proof,butterfly in zip(proofs,butterflies):
        if [butterfly['outputs'][i] for i in (1,3)]!=proof['inputs']:
            raise ValueError('butterfly outputs do not match subsequent complex inputs')
        proof['butterfly']=butterfly
    return proofs


def verify(text, source):
    contracts = contract(source)
    source_contract = contracts['raw_product']
    copies = verify_copies(text)
    executed, reads = rows(text,'mu4_raw_execute'), rows(text,'mu4_product_read')
    terminal = rows(text,'mu4_product_terminal')
    if len(terminal)!=1 or terminal[0].get('closed')!='1' or terminal[0].get('capped')!='0' or int(terminal[0]['reads'])!=len(reads):
        raise ValueError('product bus capture incomplete or capped')
    if any(r.get('ownership')!='data_bus' or int(r['mask'],16)!=0xffff for r in reads):
        raise ValueError('product operand bus scope changed')
    if any(r.get('identity')!='Execute_entry' or r.get('context_only')!='1' for r in executed):
        raise ValueError('product instruction identity missing')
    starts = [i for i,r in enumerate(executed) if int(r['address'],16)==RAW_PRODUCT_BASE]
    if len(starts)!=2:
        raise ValueError('expected precisely two selected product iterations')
    proofs = []
    instruction_addresses = list(range(0x25bd2,0x25be0))
    for channel,start in enumerate(starts):
        block = executed[start:start+len(instruction_addresses)]
        if [int(r['address'],16) for r in block]!=instruction_addresses:
            raise ValueError('product prefix Execute sequence incomplete')
        for row in block:
            address = int(row['address'],16)
            if int(row['opcode'],16)!=RAW_PRODUCT_WITNESS[address-RAW_PRODUCT_BASE] or int(row['st1'],16)!=0x6100:
                raise ValueError('product opcode or status regime changed')
        begin,end = float(block[0]['t']),float(block[-1]['t'])
        if begin>=end:
            raise ValueError('product iteration time ordering invalid')
        # Retain actual reads belonging to this prefix; never substitute live AR values.
        values = {}
        for read in reads:
            if begin<=float(read['t'])<=end:
                address,value = int(read['address'],16),int(read['value'],16)
                if address in values and values[address]!=value:
                    raise ValueError('product operand changed within selected prefix')
                values[address] = value
        base = (0x7880,0xe880)[channel]
        def pair(address):
            try:
                return (values[address]<<16)|values[address+1]
            except KeyError as error:
                raise ValueError(f'missing product operand pair {address:04x}') from error
        x,y,c,d = pair(base),pair(base-2),pair(0xf000),pair(0xf0fe)
        expected = reference_raw_product(x,y,c,d)
        for row in block:
            for accumulator,value in expected.get(int(row['address'],16),{}).items():
                if int(row[accumulator],16)!=value:
                    raise ValueError('native partial-product intermediate differs from scalar contract')
        result = expected[0x25bdf]['b'] & 0xffffffff
        first_copy = copies['transfers'][(0,3)[channel]]
        if result!=int(first_copy['words'],16) or float(first_copy['read_time'])<=end:
            raise ValueError('helper result does not match subsequent copied raw words')
        proofs.append({'channel':channel,'input':f'{x:08x}','other_input':f'{y:08x}',
                       'coefficients':[f'{c:08x}',f'{d:08x}'],'result':f'{result:08x}',
                       'validated_intermediate_registers':sum(map(len,expected.values()))})
    upstream=verify_upstream(text,proofs)
    return {'iterations':proofs,'source_contract':source_contract,'upstream':upstream,
            'selected_partial_product_arithmetic_validated':True,'native_audio_validated':False}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log',type=Path)
    parser.add_argument('--source-a00',type=Path,required=True)
    args=parser.parse_args()
    try:
        print(json.dumps(verify(args.log.read_text(),args.source_a00.read_bytes()),indent=2))
    except (ValueError,KeyError,OSError) as error:
        print(f'FAIL: {error}',file=sys.stderr)
        return 1
    return 0


if __name__=='__main__':
    sys.exit(main())
