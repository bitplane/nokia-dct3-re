"""Check recovered NSM-3 compact self-test receive instructions."""
import argparse
import hashlib
from pathlib import Path
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_BIG_ENDIAN


def verify(image):
    if hashlib.sha1(image).hexdigest() != 'c1a0fe95cedb89a92b19654208cc4855e1a4988e':
        raise ValueError('requires acquired 8210 v5.31 PPM C')
    decoder = Cs(CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_BIG_ENDIAN)
    instructions = {}
    for low, high in ((0x243a02, 0x243a44), (0x240dc6, 0x240dd6),
                      (0x240e20, 0x240e8e)):
        instructions.update((i.address, (i.mnemonic, i.op_str))
                            for i in decoder.disasm(image[low-0x200000:high-0x200000], low))
    # Sum the whole subtract cascade to class 74; command 0a + 3 is 0d.
    expected = {
        0x243a02: ('ldrb', 'r0, [r4, #3]'),
        0x243a04: ('subs', 'r0, r0, #3'),
        0x243a0a: ('subs', 'r0, #2'),
        0x243a10: ('subs', 'r0, #0xc'),
        0x243a16: ('subs', 'r0, #2'),
        0x243a1c: ('subs', 'r0, #0x2d'),
        0x243a22: ('subs', 'r0, #2'),
        0x243a28: ('subs', 'r0, #0x32'),
        0x243a2c: ('beq', '#0x243a36'),
        0x243a3e: ('bl', '#0x240db0'),
        0x240dc6: ('ldrb', 'r1, [r4, #8]'),
        0x240dc8: ('movs', 'r0, #0xa'),
        0x240dd0: ('subs', 'r0, #3'),
        0x240dd4: ('beq', '#0x240e20'),
        0x240e24: ('lsrs', 'r0, r0, #3'),
        0x240e26: ('blo', '#0x240ec4'),
        0x240e2a: ('bl', '#0x287848'),
        0x240e3a: ('strb', 'r6, [r7, #0xf]'),
        0x240e48: ('ldrb', 'r0, [r4, #9]'),
        0x240e4e: ('strb', 'r6, [r7, #0x10]'),
        0x240e64: ('strb', 'r6, [r7, #0x11]'),
    }
    for address, expected_instruction in expected.items():
        if instructions.get(address) != expected_instruction:
            raise ValueError(f'contract instruction mismatch at {address:08x}')
    for address, value in ((0x2411c8, 0x13fde1), (0x2411d0, 0x13fbe0)):
        if int.from_bytes(image[address-0x200000:address-0x200000+4], 'big') != value:
            raise ValueError(f'contract literal mismatch at {address:08x}')
    if image[0x13ee78:0x13ee78+25] != bytes.fromhex(
            '3e3e3e3e3e11190102030e170405060f18070809101a0c0a0b'):
        raise ValueError('own NSM-3 matrix table mismatch')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('flash', type=Path)
    args = parser.parse_args()
    try:
        verify(args.flash.read_bytes())
    except (OSError, ValueError) as error:
        parser.exit(1, f'8210 self-test contract FAIL: {error}\n')
    print('8210 own class-74/command-0d compact consumer PASS')
