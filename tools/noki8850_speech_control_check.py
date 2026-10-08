"""Recover the acquired NSM-2 speech field without sibling-ROM assumptions."""
import argparse
import hashlib
from pathlib import Path
import re

from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_BIG_ENDIAN


def recover(image):
    if hashlib.sha1(image).hexdigest() != '9f966787403b68a09530680ad911302403eb1521':
        raise ValueError('requires acquired NSM-2 v5.31 MCU')

    def read(address, size):
        offset = address - 0x200000
        if offset < 0 or offset + size > len(image):
            raise ValueError('contract address outside image')
        return image[offset:offset + size]

    def u32(address):
        return int.from_bytes(read(address, 4), 'big')

    expected = {
        0x2cb0de: ('cmp', 'r0, #0x34'),
        0x2cb334: ('lsls', 'r0, r6, #9'),
        0x2cb33c: ('lsls', 'r0, r0, #9'),
        0x2cb33e: ('ldr', 'r1, [pc, #0x384]'),
        0x2cb2f4: ('ldr', 'r6, [pc, #0x38]'),
        0x2cb2f6: ('ldrh', 'r7, [r6, #2]'),
        0x2cb2f8: ('ands', 'r1, r7'),
        0x2cb2e6: ('strh', 'r0, [r6, #2]'),
        0x2cb37c: ('ldr', 'r0, [pc, #0x36c]'),
        0x2cb384: ('ldr', 'r1, [pc, #0x368]'),
        0x2cb204: ('strh', 'r0, [r1]'),
        0x2cb206: ('ldr', 'r2, [pc, #0x3a8]'),
        0x2cb3ca: ('strh', 'r0, [r2]'),
    }
    decoder = Cs(CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_BIG_ENDIAN)
    for address, pair in expected.items():
        instruction = next(decoder.disasm(read(address, 4), address), None)
        if instruction is None or (instruction.mnemonic, instruction.op_str) != pair:
            raise ValueError(f'control instruction differs at {address:x}')
    for selector, target in ((8, 0x2cb37c), (17, 0x2cb334)):
        if u32(0x2cb0ec + selector * 4) != target:
            raise ValueError('selector table differs')
    literals = {0x2cb330: 0x135664, 0x2cb6c4: 0xfdff,
                0x2cb6e8: 0x135668, 0x2cb6ec: 0xffff8000,
                0x2cb5b0: 0x100a8}
    if any(u32(address) != value for address, value in literals.items()):
        raise ValueError('control literals differ')
    return {'command': 8, 'field_selector': 17, 'field': 0x0200,
            'field_shadow': 0x135666, 'command_shadow': 0x135668,
            'writer': 0x2cb3ca, 'runtime_validated': False,
            'native_speech_validated': False}


def verify_call(text):
    cursor = 0
    for pattern in (
        r'8850_call_physical: action=send\b',
        r'dsp_control_write: data=860b pc=002cb3ca r4=00000008 ',
        r'8850_call_physical: action=end\b',
        r'dsp_control_write: data=840a pc=002cb3ca r4=00000008 ',
    ):
        match = re.search(pattern, text[cursor:])
        if match is None:
            raise ValueError(f'missing ordered call-control event: {pattern}')
        cursor += match.end()
    return {'call_field': 0x0200, 'runtime_validated': True,
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
