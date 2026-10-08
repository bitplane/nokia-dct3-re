"""Recover NPE-3 parameter encoding; bit 0x0200 is not yet speech-validated."""
import hashlib
import re
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_BIG_ENDIAN


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
    calls = []
    for offset in range(0, len(image) - 3, 2):
        first = int.from_bytes(image[offset:offset + 2], 'big')
        second = int.from_bytes(image[offset + 2:offset + 4], 'big')
        if first & 0xf800 != 0xf000 or second & 0xf800 != 0xf800:
            continue
        displacement = ((first & 0x7ff) << 12) | ((second & 0x7ff) << 1)
        if displacement & 0x400000:
            displacement -= 0x800000
        address = 0x200000 + offset
        if address + 4 + displacement == 0x426eb4:
            calls.append(address)
    expected_calls = [0x303278, 0x303c7c, 0x303c86, 0x41ab0e, 0x41b07a,
                      0x420c0a, 0x420c3e, 0x420c66, 0x4272ae, 0x45dbfc, 0x46b9ee]
    if calls != expected_calls:
        raise ValueError('direct Thumb BL candidate inventory differs')
    return {'compiler': 0x426eb4, 'selector_count': 55,
            'field_selector': 0x11, 'field_mask': 0x0200,
            'keep_mask': 0xfdff, 'parameter_selector': 8,
            'parameter_shadow': 0x16ffe6,
            'direct_bl_candidates': calls,
            'indirect_call_coverage': False,
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
