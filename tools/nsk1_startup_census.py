"""Own NSK-1 report-15 flag inputs and quantified direct-call census."""
import argparse
import json
from pathlib import Path
import struct
import sys

from capstone import Cs,CS_ARCH_ARM,CS_MODE_THUMB,CS_MODE_BIG_ENDIAN
from capstone.arm import ARM_OP_MEM,ARM_REG_PC,ARM_REG_R0,ARM_REG_R1

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsk1_static_check import inspect
from tools.message_census import decode_image,extract_calls
from tools.nse5_roller_contract import thumb_bl_candidates


def power_window_contract(image,variant):
    shift = {'standard':0,'2915':-0x10}[variant]
    fragments = (
        (0x26276c,'2007f030f8788028'),
        (0x26278e,'4898434821e8f033f83004080c008028'),
        (0x262814,'2000882922e100d24291db0b4a754291dc08882122e100d24291db034a714291dc002001bd70'),
        (0x22a13a,'f038fb7e48a446804e7578302800d1074f9c20ef78394008703821284689e01b'),
    )
    for address,encoded in fragments:
        start = address+shift-0x200000
        expected = bytes.fromhex(encoded)
        if image[start:start+len(expected)] != expected:
            raise ValueError('own power window instruction contract differs')
    def word(address):
        offset = address+shift-0x200000
        if offset < 0 or offset+4 > len(image):
            raise ValueError('power window literal outside image')
        return struct.unpack_from('>I',image,offset)[0]
    multiplier,upper = word(0x2629f0),word(0x2629f8)
    if (multiplier,upper,word(0x22a318)) != (1500,5500,0x10a051):
        raise ValueError('power window constants or countdown differ')
    table = word(0x29290c)
    if not 0x200000 <= table <= 0x200000+len(image)-10:
        raise ValueError('own ADC source table outside image')
    channels = list(image[table-0x200000:table-0x200000+10])
    if channels != [0,4,5,6,7,3,1,2,2,1]:
        raise ValueError('own ADC source map differs')
    return {'source':7,'adc_channel':channels[7], 'source_table':hex(table),
            'calibrated_sample_multiplier':multiplier,'divisor':232,
            'inclusive_window':[1800,upper],'countdown':hex(word(0x22a318)),
            'report_callsite':hex(0x22a1c0+shift),
            'scope':'own firmware conversion/window and report gate; not physical units, pack calibration or full lifecycle closure'}


def flag_contract(image,variant):
    shift = {'standard':0,'2915':-0x10}[variant]
    table = 0x2415d8+shift
    if len(image) < 0x4189c+shift:
        raise ValueError('truncated own flag-table contract')
    flag_base = struct.unpack_from('>I',image,0x41898+shift)[0]
    if not 0x100000 <= flag_base <= 0x110000-11:
        raise ValueError('flag array outside bounded own RAM')
    decoder = Cs(CS_ARCH_ARM,CS_MODE_THUMB|CS_MODE_BIG_ENDIAN)
    decoder.detail = True
    dispatcher = list(decoder.disasm(image[0x415be+shift:0x415ce+shift],0x2415be+shift))
    if [(i.mnemonic,i.op_str) for i in dispatcher] != [
            ('movs','r0, #0xa'),('subs','r0, r1, r0'),('cmp','r0, #0xb'),
            ('bls',f'#0x{0x2415ce+shift:x}'),('subs','r0, #0x5a'),('cmp','r0, #0'),
            ('beq',f'#0x{0x241608+shift:x}'),('b',f'#0x{0x241690+shift:x}')]:
        raise ValueError('flag input/reset dispatch differs')
    rows = []
    for index,pointer in enumerate(struct.unpack_from('>12I',image,table-0x200000)):
        row = {'input':hex(index+10),'handler':hex(pointer),'flag_slot':None}
        if pointer != 0x241690+shift:
            offset = pointer-0x200000
            if offset < 0 or offset+8 > len(image) or pointer & 1:
                raise ValueError('flag handler outside image or unaligned')
            code = list(decoder.disasm(image[offset:offset+8],pointer))
            if len(code) != 4 or [i.mnemonic for i in code[:3]] != ['ldr','movs','strb']:
                raise ValueError('flag handler instruction shape differs')
            load,value,store,branch = code
            if (load.operands[0].reg != ARM_REG_R1 or load.operands[1].type != ARM_OP_MEM
                    or load.operands[1].mem.base != ARM_REG_PC
                    or value.op_str != 'r0, #1' or store.operands[0].reg != ARM_REG_R0
                    or store.operands[1].type != ARM_OP_MEM or store.operands[1].mem.base != ARM_REG_R1
                    or not ((branch.mnemonic == 'b' and branch.op_str == f'#0x{0x241672+shift:x}')
                            or (branch.address == 0x241672+shift
                                and branch.mnemonic == 'movs' and branch.op_str == 'r0, #0'))):
                raise ValueError('flag handler register/value/continuation differs')
            pool = ((pointer+4)&~3)+load.operands[1].mem.disp-0x200000
            if pool < 0 or pool+4 > len(image) or struct.unpack_from('>I',image,pool)[0] != flag_base:
                raise ValueError('flag handler does not address own array')
            row['flag_slot'] = store.operands[1].mem.disp
        rows.append(row)
    if sorted(row['flag_slot'] for row in rows if row['flag_slot'] is not None) != list(range(11)):
        raise ValueError('eleven distinct flag slots not covered')
    return {'entry':hex(0x2415ac+shift),'flag_base':hex(flag_base),'reset_input':'0x64',
            'inputs':rows,'report_callsite':hex(0x24168c+shift)}


def analyze(image,variant):
    inspect(image,variant)
    contract = flag_contract(image,variant)
    target = int(contract['entry'],16)
    candidates = thumb_bl_candidates(image,(target,))[target]
    swapped = bytearray(image)
    swapped[::2],swapped[1::2] = image[1::2],image[::2]
    profile = {'apis':[{'address':hex(target),'name':'nsk1_ready_flag','kind':'callback','arguments':{'input':'r0'}}]}
    calls = extract_calls(profile,decode_image(swapped,0x200000),swapped,0x200000)
    mapping = {int(row['input'],16):row['flag_slot'] for row in contract['inputs']}
    decoded_sites = {call['callsite'] for call in calls}
    return {'contract':contract,'power_window':power_window_contract(image,variant),
            'coverage':{'aligned_halfwords_scanned':(len(image)-2)//2,
                        'direct_bl_candidates':len(candidates),'decoded_calls':len(calls),
                        'argument_recovered':sum(call['arguments']['input'] is not None for call in calls),
                        'not_decoded_candidates':[hex(site) for site in candidates if site not in decoded_sites]},
            'producers':[{'callsite':hex(call['callsite']),
                          'input':None if call['arguments']['input'] is None else hex(call['arguments']['input']),
                          'flag_slot':mapping.get(call['arguments']['input']),
                          'action':'reset' if call['arguments']['input']==0x64 else 'flag input'} for call in calls],
            'scope':'direct Thumb-BL candidates and literal argument recovery; no indirect, table-driven or ARM-call closure'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image',type=Path)
    parser.add_argument('--variant',choices=('standard','2915'),default='standard')
    args = parser.parse_args()
    try:
        report = analyze(args.image.read_bytes(),args.variant)
    except (OSError,ValueError,struct.error) as error:
        parser.exit(1,f'NSK-1 startup census failed: {error}\n')
    print(json.dumps(report,indent=2))


if __name__ == '__main__':
    main()
