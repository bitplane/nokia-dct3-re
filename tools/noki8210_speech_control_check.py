"""Recover own NSM-3 control encoding and physical-call correspondence."""
import argparse
import hashlib
from pathlib import Path
import re

from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_BIG_ENDIAN


def recover(image):
    if hashlib.sha1(image).hexdigest() != 'c1a0fe95cedb89a92b19654208cc4855e1a4988e':
        raise ValueError('requires acquired NSM-3 v5.31 PPM C')

    def read(address, size):
        offset = address - 0x200000
        if offset < 0 or offset + size > len(image):
            raise ValueError('contract address outside image')
        return image[offset:offset + size]

    def u32(address):
        return int.from_bytes(read(address, 4), 'big')

    expected = {
        0x2caffe: ('cmp', 'r0, #0x34'),
        0x2cb254: ('lsls', 'r0, r6, #9'),
        0x2cb25c: ('lsls', 'r0, r0, #9'),
        0x2cb25e: ('ldr', 'r1, [pc, #0x384]'),
        0x2cb216: ('ldrh', 'r7, [r6, #2]'),
        0x2cb218: ('ands', 'r1, r7'),
        0x2cb206: ('strh', 'r0, [r6, #2]'),
        0x2cb29c: ('ldr', 'r0, [pc, #0x36c]'),
        0x2cb2a4: ('ldr', 'r1, [pc, #0x368]'),
        0x2cb2ea: ('strh', 'r0, [r2]'),
        0x2c6cf8: ('ldrh', 'r1, [r4]'),
        0x2c6cfa: ('ands', 'r0, r1'),
        0x2c6d02: ('ldr', 'r2, [pc, #0x3a4]'),
        0x2c6d0a: ('ldrh', 'r1, [r2, r1]'),
        0x2c6d0c: ('orrs', 'r1, r0'),
        0x2c6d12: ('strh', 'r1, [r4]'),
        0x2c732e: ('ldrh', 'r1, [r4]'),
        0x2c733e: ('bl', '#0x2cafd8'),
    }
    decoder = Cs(CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_BIG_ENDIAN)
    for address, pair in expected.items():
        instruction = next(decoder.disasm(read(address, 4), address), None)
        if instruction is None or (instruction.mnemonic, instruction.op_str) != pair:
            raise ValueError(f'control instruction differs at {address:x}')
    if u32(0x2cb00c + 17 * 4) != 0x2cb254 or u32(0x2cb00c + 8 * 4) != 0x2cb29c:
        raise ValueError('selector table differs')
    literals = {0x2cb5e4: 0xfdff, 0x2cb250: 0x135774,
                0x2cb608: 0x135778, 0x2cb60c: 0xffff8000,
                0x2cb4d0: 0x100a8, 0x2c70a4: 0x337db8,
                0x2c70a8: 0x337db0, 0x2c76c8: 0x337da4}
    if any(u32(address) != value for address, value in literals.items()):
        raise ValueError('control literals differ')
    if read(0x337db0, 10) != bytes.fromhex('0200020002000200fdff'):
        raise ValueError('publisher field table differs')
    if read(0x337da4, 3) != bytes.fromhex('080930'):
        raise ValueError('publisher command selectors differ')
    return {'command': 8, 'field_selector': 17, 'field': 0x0200,
            'field_shadow': 0x135776, 'command_shadow': 0x135778,
            'publisher_field_table': 0x337db0, 'pcm_validated': False}


def verify_call(text):
    cursor = 0
    for pattern in (
        r'8210_call_physical: action=send',
        r'dsp_control_write: data=860b pc=002cb2ea r4=00000008 ',
        r'8210_call_physical: action=end',
        r'dsp_control_write: data=840a pc=002cb2ea r4=00000008 ',
    ):
        match = re.search(pattern, text[cursor:])
        if match is None:
            raise ValueError(f'missing ordered call-control event: {pattern}')
        cursor += match.end()
    return {'call_field': 0x0200, 'native_speech_validated': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', type=Path)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    print(recover(args.rom.read_bytes()))
    print(verify_call(args.log.read_text(errors='replace')))


if __name__ == '__main__':
    main()
