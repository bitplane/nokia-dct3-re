"""Recover NPE-3 encoding and paired mode data, not PCM clock semantics."""
import hashlib
import argparse
import json
from pathlib import Path
import re
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_BIG_ENDIAN


def parse_mode_pairs(data):
    if len(data) != 9 * 4:
        raise ValueError('paired mode table requires nine four-byte records')
    return [{'mode': index + 1,
             'command8': int.from_bytes(data[index * 4:index * 4 + 2], 'big'),
             'command9': int.from_bytes(data[index * 4 + 2:index * 4 + 4], 'big')}
            for index in range(9)]


def recover(image):
    if hashlib.sha1(image).hexdigest() != '3d9ea319503e78ec69b60d72cda23e461e118ea9':
        raise ValueError('requires acquired NPE-3 v5.56 PPM C')

    def read(address, size):
        offset = address - 0x200000
        if offset < 0 or offset + size > len(image):
            raise ValueError('control contract address outside image')
        return image[offset:offset + size]

    def u32(address):
        return int.from_bytes(read(address, 4), 'big')

    expected = {
        0x303c30: ('push', '{r4, r5, lr}'),
        0x303c3e: ('cmp', 'r5, #0xa'),
        0x303c40: ('bge', '#0x303c8e'),
        0x303c42: ('cmp', 'r5, #0'),
        0x303c44: ('beq', '#0x303c90'),
        0x303c6e: ('ldr', 'r1, [pc, #0x378]'),
        0x303c70: ('lsls', 'r0, r5, #2'),
        0x303c72: ('adds', 'r4, r1, r0'),
        0x303c74: ('subs', 'r4, #4'),
        0x303c76: ('ldrh', 'r1, [r4]'),
        0x303c78: ('movs', 'r0, #8'),
        0x303c7c: ('bl', '#0x426eb4'),
        0x303c80: ('ldrh', 'r1, [r4, #2]'),
        0x303c82: ('movs', 'r0, #9'),
        0x303c86: ('bl', '#0x426eb4'),
        0x305668: ('ldrb', 'r0, [r4, #9]'),
        0x30566a: ('bl', '#0x303c30'),
        0x305272: ('ldrb', 'r5, [r4, #8]'),
        0x30528c: ('adds', 'r2, r5, #0'),
        0x30532c: ('movs', 'r0, #0x73'),
        0x30532e: ('subs', 'r0, r2, r0'),
        0x305336: ('subs', 'r0, #3'),
        0x305338: ('cmp', 'r0, #0x36'),
        0x30533a: ('bls', '#0x3053a6'),
        0x3053a6: ('adr', 'r1, #8'),
        0x3053a8: ('lsls', 'r0, r0, #2'),
        0x3053aa: ('ldr', 'r0, [r1, r0]'),
        0x3053ac: ('mov', 'pc, r0'),
        0x426efa: ('cmp', 'r0, #0x36'),
        0x426efe: ('adr', 'r2, #8'),
        0x4271d4: ('lsls', 'r2, r4, #9'),
        0x4271d6: ('rsbs', 'r2, r2, #0'),
        0x4271da: ('lsrs', 'r2, r2, #0x1f'),
        0x4271dc: ('lsls', 'r2, r2, #9'),
        0x4271e2: ('ldr', 'r2, [pc, #0x368]'),
        0x42719e: ('ldrh', 'r5, [r3, #2]'),
        0x42719c: ('ldr', 'r3, [pc, #0x70]'),
        0x4271a0: ('ands', 'r2, r5'),
        0x42718a: ('strh', 'r4, [r3, #2]'),
        0x42722e: ('ldr', 'r2, [pc, #0x344]'),
        0x427230: ('lsls', 'r3, r4, #0x14'),
        0x427232: ('lsrs', 'r3, r3, #0x14'),
        0x427234: ('orrs', 'r2, r3'),
        0x42723a: ('ldr', 'r2, [pc, #0x33c]'),
    }
    decoder = Cs(CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_BIG_ENDIAN)
    for address, value in expected.items():
        instruction = next(decoder.disasm(read(address, 4), address), None)
        if instruction is None or (instruction.mnemonic, instruction.op_str) != value:
            raise ValueError(f'control instruction differs at {address:x}')
    table = [u32(0x426f08 + 4 * index) for index in range(55)]
    if table[0x11] != 0x4271d4 or table[8] != 0x42722e:
        raise ValueError('control selector routing differs')
    if u32(0x42754c) != 0xfdff or u32(0x427574) != 0xffff8000:
        raise ValueError('field keep-mask or command encoding differs')
    if u32(0x427210) != 0x16ffe4 or u32(0x427578) != 0x16ffe6:
        raise ValueError('parameter-shadow ownership differs')
    if u32(0x303fe8) != 0x262444:
        raise ValueError('paired mode-table pointer differs')
    modes = parse_mode_pairs(read(0x262444, 36))
    dispatch = [u32(0x3053b0 + index * 4) for index in range(55)]
    if [index for index, target in enumerate(dispatch) if target == 0x305668] != [10]:
        raise ValueError('paired mode consumer dispatch differs')
    calls = []
    mode_calls = []
    for offset in range(0, len(image) - 3, 2):
        first = int.from_bytes(image[offset:offset + 2], 'big')
        second = int.from_bytes(image[offset + 2:offset + 4], 'big')
        if first & 0xf800 != 0xf000 or second & 0xf800 != 0xf800:
            continue
        displacement = ((first & 0x7ff) << 12) | ((second & 0x7ff) << 1)
        if displacement & 0x400000:
            displacement -= 0x800000
        address = 0x200000 + offset
        target = address + 4 + displacement
        if target == 0x426eb4:
            calls.append(address)
        if target == 0x303c30:
            mode_calls.append(address)
    expected_calls = [0x303278, 0x303c7c, 0x303c86, 0x41ab0e, 0x41b07a,
                      0x420c0a, 0x420c3e, 0x420c66, 0x4272ae, 0x45dbfc, 0x46b9ee]
    if calls != expected_calls:
        raise ValueError('direct Thumb BL candidate inventory differs')
    if mode_calls != [0x30566a]:
        raise ValueError('paired mode direct-call inventory differs')
    return {'compiler': 0x426eb4, 'selector_count': 55,
            'field_selector': 0x11, 'field_mask': 0x0200,
            'keep_mask': 0xfdff, 'parameter_selector': 8,
            'parameter_shadow': 0x16ffe6,
            'direct_bl_candidates': calls,
            'indirect_call_coverage': False,
            'paired_mode_publisher': 0x303c30,
            'paired_mode_table': 0x262444,
            'paired_modes': modes,
            'paired_mode_semantics_validated': False,
            'negative_mode_input_rejected': False,
            'paired_mode_message_byte': 9,
            'paired_mode_command_byte': 8,
            'paired_mode_command': 0x73 + 3 + 10,
            'paired_mode_consumer': 0x305258,
            'paired_mode_dispatch_table': 0x3053b0,
            'paired_mode_dispatch_records_checked': 55,
            'paired_mode_direct_bl_candidates': mode_calls,
            'speech_semantics_validated': False}


def verify_call_field(log):
    """Pin observed call/teardown selection, not PCM or native DSP execution."""
    cursor = 0
    for pattern in (
            r'6210_call_physical: action=send\b',
            r'dsp_control_write: data=870b pc=0042727c r4=0000870b r7=00000008\b',
            r'6210_call_physical: action=end\b',
            r'dsp_control_write: data=850a pc=0042727c r4=0000850a r7=00000008\b'):
        match = re.search(pattern, log[cursor:])
        if not match:
            raise ValueError('missing ordered physical call field evidence: ' + pattern)
        cursor += match.end()
    return {'parameter_command': 8, 'call_field': 0x0200,
            'pcm_validated': False, 'native_speech_validated': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('--log', type=Path)
    args = parser.parse_args()
    try:
        result = {'static': recover(args.image.read_bytes())}
        if args.log:
            result['physical_call'] = verify_call_field(args.log.read_text(errors='replace'))
        print(json.dumps(result, indent=2))
    except (OSError, ValueError) as error:
        parser.exit(1, f'NPE-3 DSP control FAIL: {error}\n')


if __name__ == '__main__':
    main()
