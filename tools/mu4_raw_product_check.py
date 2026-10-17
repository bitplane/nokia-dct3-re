"""Validate two native partial-product prefixes; no full decoder acceptance."""
import argparse
import json
from pathlib import Path
import sys

if __package__:
    from .mu4_playback_frame_check import rows
    from .mu4_raw_output_check import verify as verify_copies
    from .mu4_context_contract import contract, RAW_PRODUCT_BASE, RAW_PRODUCT_WITNESS, reference_raw_product
else:
    from mu4_playback_frame_check import rows
    from mu4_raw_output_check import verify as verify_copies
    from mu4_context_contract import contract, RAW_PRODUCT_BASE, RAW_PRODUCT_WITNESS, reference_raw_product


def verify(text, source):
    source_contract = contract(source)['raw_product']
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
    return {'iterations':proofs,'source_contract':source_contract,
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
