"""Recover NSB-7 v5.22 speech control; not PCM or native speech proof."""
import argparse
import hashlib
from pathlib import Path
import re

from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_BIG_ENDIAN


def recover(image):
    if hashlib.sha1(image).hexdigest() != '4dacc50f57fe295168a53d3e5855a81acf35ab96':
        raise ValueError('requires acquired NSB-7 v5.22 MCU')

    def read(address, size):
        offset = address - 0x200000
        if offset < 0 or offset + size > len(image):
            raise ValueError('contract address outside image')
        return image[offset:offset + size]

    decoder = Cs(CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_BIG_ENDIAN)
    instructions = {
        0x2cb652: ('cmp', 'r0, #0x34'),
        0x2cb8a8: ('lsls', 'r0, r6, #9'),
        0x2cb8b0: ('lsls', 'r0, r0, #9'),
        0x2cb8b2: ('ldr', 'r1, [pc, #0x384]'),
        0x2cb868: ('ldr', 'r6, [pc, #0x38]'),
        0x2cb86a: ('ldrh', 'r7, [r6, #2]'),
        0x2cb86c: ('ands', 'r1, r7'),
        0x2cb85a: ('strh', 'r0, [r6, #2]'),
        0x2cb8f0: ('ldr', 'r0, [pc, #0x36c]'),
        0x2cb8f8: ('ldr', 'r1, [pc, #0x368]'),
        0x2cb778: ('strh', 'r0, [r1]'),
        0x2cb77a: ('ldr', 'r2, [pc, #0x20c]'),
        0x2cb93e: ('strh', 'r0, [r2]'),
    }
    for address, expected in instructions.items():
        instruction = next(decoder.disasm(read(address, 4), address), None)
        if instruction is None or (instruction.mnemonic, instruction.op_str) != expected:
            raise ValueError(f'control instruction differs at {address:x}')
    literals = {0x2cb8a4: 0x12df00, 0x2cbc38: 0xfdff,
                0x2cbc60: 0xffff8000, 0x2cbc64: 0x12df02,
                0x2cb988: 0x100a8,
                0x2cb660 + 8 * 4: 0x2cb8f0,
                0x2cb660 + 17 * 4: 0x2cb8a8}
    if any(int.from_bytes(read(address, 4), 'big') != value
           for address, value in literals.items()):
        raise ValueError('control table or literals differ')
    return {'command': 8, 'field_selector': 17, 'field': 0x0200,
            'field_shadow': 0x12df02, 'writer': 0x2cb93e,
            'pcm_validated': False, 'native_speech_validated': False}


def verify_call(text, *, incoming=False):
    cursor = 0
    for pattern in (
        r'nsb7_call_input: key=Send state=1 ' if incoming else
        r'nsb7_outgoing_call_input: key=Send state=1 ',
        r'nsb7_key_decode: key=0e observer_writes=0',
        r'dsp_control_write: data=860b pc=002cb93e r4=00000008 ',
        r'nsb7_call_input: key=End state=1 ' if incoming else
        r'nsb7_outgoing_call_input: key=End state=1 ',
        r'nsb7_key_decode: key=0f observer_writes=0',
        r'dsp_control_write: data=840a pc=002cb93e r4=00000008 ',
    ):
        match = re.search(pattern, text[cursor:])
        if match is None:
            raise ValueError(f'missing ordered call-control event: {pattern}')
        cursor += match.end()
    return {'runtime_validated': True, 'pcm_validated': False,
            'native_speech_validated': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', type=Path)
    parser.add_argument('--log', type=Path)
    args = parser.parse_args()
    print(recover(args.rom.read_bytes()))
    if args.log:
        print(verify_call(args.log.read_text(errors='replace')))


if __name__ == '__main__':
    main()
