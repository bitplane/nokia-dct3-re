"""Check the own NSB-6 receive-side compact self-test contract."""
import argparse
import hashlib
from pathlib import Path

from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_BIG_ENDIAN


def verify(image):
    if hashlib.sha1(image).hexdigest() != 'a214a0d69760ecd8eeca0b9d82f95c94bdfe70ed':
        raise ValueError('requires acquired 8890 v12.20 PPM C')
    decoder = Cs(CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_BIG_ENDIAN)
    instructions = {}
    for low, high in ((0x243706, 0x243748), (0x240952, 0x240968),
                      (0x2409ac, 0x240a0e)):
        instructions.update((i.address, (i.mnemonic, i.op_str))
                            for i in decoder.disasm(image[low-0x200000:high-0x200000], low))
    # These selected instructions anchor the control-flow reading documented
    # in 8xxx_bringup, rather than treating a sibling signature as semantics.
    expected = {
        0x243706: ('ldrb', 'r0, [r4, #3]'),
        0x24372c: ('subs', 'r0, #0x32'),
        0x243730: ('beq', '#0x24373a'),
        0x243742: ('bl', '#0x240938'),
        0x240952: ('ldrb', 'r1, [r4, #8]'),
        0x240954: ('movs', 'r0, #0xa'),
        0x24095c: ('subs', 'r0, #3'),
        0x240960: ('beq', '#0x2409ac'),
        0x2409b0: ('lsrs', 'r0, r0, #3'),
        0x2409b6: ('bl', '#0x280cf8'),
        0x2409c8: ('strb', 'r6, [r0, #0xf]'),
        0x2409e0: ('ldrb', 'r0, [r4, #9]'),
        0x2409e8: ('strb', 'r6, [r1, #0x10]'),
        0x2409fe: ('strb', 'r6, [r1, #0x11]'),
    }
    for address, instruction in expected.items():
        if instructions.get(address) != instruction:
            raise ValueError(f'contract instruction mismatch at {address:08x}')
    for address, value in ((0x240d18, 0x13fde1), (0x240a68, 0x13fbe0)):
        if int.from_bytes(image[address-0x200000:address-0x200000+4], 'big') != value:
            raise ValueError(f'contract literal mismatch at {address:08x}')
    if image[0x139f4c:0x139f4c+25] != bytes.fromhex(
            '3e3e3e3e3e11190102030e170405060f18070809101a0c0a0b'):
        raise ValueError('own NSB-6 decoded matrix table mismatch')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('flash', type=Path)
    args = parser.parse_args()
    try:
        verify(args.flash.read_bytes())
    except (OSError, ValueError) as error:
        parser.exit(1, f'8890 self-test contract FAIL: {error}\n')
    print('8890 own class-74/command-0d compact self-test consumer PASS')
