"""Verify the acquired NSM-3 candidate-acquisition receive contract."""

import argparse
import hashlib
from pathlib import Path
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_BIG_ENDIAN


def verify(image):
    if hashlib.sha1(image).hexdigest() != 'c1a0fe95cedb89a92b19654208cc4855e1a4988e':
        raise ValueError('requires acquired 8210 v5.31 PPM C')
    def read(address, size):
        return image[address - 0x200000:address - 0x200000 + size]
    expected = [0x307060, 0x307058, 0x30707a, 0x307050, 0x307048,
                0x307040, 0x307038, 0x307030, 0x307028, 0x307020,
                0x30707a, 0x30707a, 0x307018]
    table = [int.from_bytes(read(0x306fd4 + 4*i, 4), 'big') for i in range(13)]
    if table != expected:
        raise ValueError('own thirteen-entry RX table differs')
    decoder = Cs(CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_BIG_ENDIAN)
    def instructions(address, size):
        return [(ins.mnemonic, ins.op_str) for ins in decoder.disasm(read(address, size), address)]
    if instructions(0x30702a, 4) != [('bl', '#0x2df484')]:
        raise ValueError('type 8b handler differs')
    if instructions(0x2df498, 6) != [('movs', 'r0, #0xc'), ('bl', '#0x28845c')]:
        raise ValueError('type 8b does not post to task 12')
    if instructions(0x2df22e, 12) != [
            ('ldrb', 'r1, [r4, #4]'), ('lsls', 'r1, r1, #0x1f'),
            ('lsrs', 'r1, r1, #0x1f'), ('ldrb', 'r2, [r0, #2]'),
            ('cmp', 'r1, r2'), ('bne', '#0x2df286')]:
        raise ValueError('own type 89 pending-context correlation differs')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    args = parser.parse_args()
    try:
        verify(args.image.read_bytes())
    except (OSError, ValueError) as error:
        parser.exit(1, f'8210 radio contract FAIL: {error}\n')
    print('8210 own RX table, task-12 route and channel correlation PASS')


if __name__ == '__main__':
    main()
