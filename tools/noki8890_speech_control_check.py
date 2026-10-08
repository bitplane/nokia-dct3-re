"""Pin NSB-6 v12.20 speech-field compilation; not PCM or native speech proof."""
import argparse
import hashlib
from pathlib import Path
import re

from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_BIG_ENDIAN


def recover(image):
    if hashlib.sha1(image).hexdigest() != 'a214a0d69760ecd8eeca0b9d82f95c94bdfe70ed':
        raise ValueError('requires acquired NSB-6 v12.20 MCU')

    def read(address, size):
        offset = address - 0x200000
        if offset < 0 or offset + size > len(image):
            raise ValueError('contract address outside image')
        return image[offset:offset + size]

    def u32(address):
        return int.from_bytes(read(address, 4), 'big')

    instructions = {
        0x2c30a2: ('cmp', 'r0, #0x34'),
        0x2c32f8: ('lsls', 'r0, r6, #9'),
        0x2c3300: ('lsls', 'r0, r0, #9'),
        0x2c3302: ('ldr', 'r1, [pc, #0x384]'),
        0x2c32b8: ('ldr', 'r6, [pc, #0x38]'),
        0x2c32ba: ('ldrh', 'r7, [r6, #2]'),
        0x2c32bc: ('ands', 'r1, r7'),
        0x2c32aa: ('strh', 'r0, [r6, #2]'),
        0x2c3340: ('ldr', 'r0, [pc, #0x36c]'),
        0x2c3348: ('ldr', 'r1, [pc, #0x368]'),
        0x2c31c8: ('strh', 'r0, [r1]'),
        0x2c31ca: ('ldr', 'r2, [pc, #0x20c]'),
        0x2c338e: ('strh', 'r0, [r2]'),
    }
    decoder = Cs(CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_BIG_ENDIAN)
    for address, expected in instructions.items():
        instruction = next(decoder.disasm(read(address, 4), address), None)
        if instruction is None or (instruction.mnemonic, instruction.op_str) != expected:
            raise ValueError(f'control instruction differs at {address:x}')
    for selector, target in ((8, 0x2c3340), (17, 0x2c32f8)):
        if u32(0x2c30b0 + selector * 4) != target:
            raise ValueError('selector table differs')
    literals = {0x2c32f4: 0x134c4c, 0x2c3688: 0xfdff,
                0x2c36b0: 0xffff8000, 0x2c36b4: 0x134c4e,
                0x2c33d8: 0x100a8}
    if any(u32(address) != value for address, value in literals.items()):
        raise ValueError('control literals differ')
    return {'command': 8, 'field_selector': 17, 'field': 0x0200,
            'field_shadow': 0x134c4e, 'command_shadow': 0x134c4e,
            'writer': 0x2c338e, 'runtime_validated': False,
            'pcm_validated': False, 'native_speech_validated': False}


def verify_call(text):
    cursor = 0
    for pattern in (
        r'8890_call_physical: action=send\b',
        r'8890_keypad_decoded: key=0e\b',
        r'dsp_control_write: data=860b pc=002c338e r4=00000008 ',
        r'8890_call_physical: action=end\b',
        r'8890_keypad_decoded: key=0f\b',
        r'dsp_control_write: data=840a pc=002c338e r4=00000008 ',
    ):
        match = re.search(pattern, text[cursor:])
        if match is None:
            raise ValueError(f'missing ordered call-control event: {pattern}')
        cursor += match.end()
    return {'runtime_validated': True, 'field': 0x0200,
            'pcm_validated': False, 'native_speech_validated': False}


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
