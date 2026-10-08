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
    if instructions(0x21ef4a, 6) != [('ldr', 'r0, [r4, #8]'), ('bl', '#0x2a2250')]:
        raise ValueError('own task-12 measurement completion differs')
    if instructions(0x2a2260, 4) != [('bl', '#0x28755e')]:
        raise ValueError('own measurement parser differs')
    if instructions(0x2a2258, 8) != [
            ('ldr', 'r2, [r0, #4]'), ('cmp', 'r2, #0'),
            ('bne', '#0x2a2266'), ('ldrb', 'r0, [r0]')]:
        raise ValueError('measurement parser context selector differs')
    if instructions(0x2a2266, 8) != [
            ('ldrb', 'r0, [r0]'), ('bl', '#0x287664'), ('pop', '{pc}')]:
        raise ValueError('alternate measurement parser differs')
    if int.from_bytes(read(0x2a16b8, 4), 'big') != 0x137f58:
        raise ValueError('selected-cell decision state differs')
    if instructions(0x21f908, 6) != [('movs', 'r0, #0'), ('bl', '#0x2a1380')]:
        raise ValueError('PH_1250 decision call differs')
    if instructions(0x21f5c8, 6) != [('movs', 'r0, #3'), ('bl', '#0x2a1380')]:
        raise ValueError('PH_9000 decision call differs')
    if instructions(0x2a18d0, 8) != [
            ('movs', 'r1, #2'), ('bics', 'r0, r1'),
            ('cmp', 'r0, #0'), ('bne', '#0x2a194c')]:
        raise ValueError('selected-cell outcome rejection differs')
    if [int.from_bytes(read(address, 4), 'big') for address in
            (0x2a18f8, 0x2a1c14, 0x2a1c18)] != [0x13722c, 0x137238, 0x137240]:
        raise ValueError('selected-cell outcome/link roots differ')
    if [int.from_bytes(read(address, 4), 'big') for address in
            (0x258e20, 0x258e28, 0x258e10)] != [0x7d4, 0x7da, 0x3ed]:
        raise ValueError('readiness producer status/input literals differ')
    if instructions(0x258aea, 6) != [
            ('movs', 'r0, #0xfb'), ('lsls', 'r0, r0, #2'), ('b', '#0x258afc')]:
        raise ValueError('07d4 readiness input construction differs')
    if instructions(0x258afc, 4) != [('bl', '#0x2aede0')]:
        raise ValueError('readiness constructor boundary differs')
    if int.from_bytes(read(0x209e44, 4), 'big') != 0x9c6:
        raise ValueError('upstream readiness cascade origin differs')
    if instructions(0x209b9a, 28) != [
            ('subs', 'r0, #1'), ('cmp', 'r0, #0'), ('beq', '#0x209bdc'),
            ('subs', 'r0, #1'), ('cmp', 'r0, #0'), ('beq', '#0x209bd8'),
            ('subs', 'r0, #4'), ('cmp', 'r0, #0'), ('beq', '#0x209bd4'),
            ('subs', 'r0, #0x2c'), ('cmp', 'r0, #0'), ('beq', '#0x209bd0'),
            ('subs', 'r0, #1'), ('cmp', 'r0, #0')]:
        raise ValueError('upstream readiness cascade arithmetic differs')
    if [int.from_bytes(read(address, 4), 'big') for address in
            (0x209c2c, 0x209e58)] != [0x7d4, 0x7da]:
        raise ValueError('09c8/09cc readiness mapping literals differ')
    if instructions(0x209bd4, 8) != [
            ('ldr', 'r0, [pc, #0x280]'), ('b', '#0x209be2'),
            ('ldr', 'r0, [pc, #0x50]'), ('b', '#0x209be2')]:
        raise ValueError('09c8/09cc readiness mapping branches differ')
    if instructions(0x209be2, 6) != [
            ('movs', 'r1, #0'), ('bl', '#0x20925c')]:
        raise ValueError('upstream readiness publication boundary differs')
    if instructions(0x210fd0, 6) != [
            ('ldr', 'r0, [sp, #4]'), ('bl', '#0x209b90')]:
        raise ValueError('observed upstream readiness caller differs')
    for address in (0x2280c8, 0x22923e):
        if instructions(address, 4) != [('bl', '#0x2252cc')]:
            raise ValueError('observed readiness input constructor caller differs')
    if [int.from_bytes(read(address, 4), 'big') for address in
            (0x228440, 0x2295dc)] != [0x9cc, 0x9c8]:
        raise ValueError('observed readiness input producer literals differ')
    if instructions(0x225362, 8) != [
            ('movs', 'r0, #0xe'), ('adds', 'r1, r4, #0'),
            ('bl', '#0x28845c')]:
        raise ValueError('readiness input mailbox publication differs')
    if instructions(0x22803e, 6) != [
            ('ldrb', 'r0, [r4, #0x11]'), ('cmp', 'r0, #1'),
            ('beq', '#0x228080')]:
        raise ValueError('readiness producer context selector differs')
    if instructions(0x228052, 4) != [('bl', '#0x229224')]:
        raise ValueError('zero context selector readiness branch differs')
    if int.from_bytes(read(0x2272e0, 4), 'big') != 0x1587:
        raise ValueError('readiness selector setter input differs')
    if instructions(0x227110, 10) != [
            ('bl', '#0x225688'), ('adds', 'r7, r0, #0'),
            ('ldr', 'r0, [pc, #0x1c8]'), ('cmp', 'r7, r0')]:
        raise ValueError('readiness selector receive predicate differs')
    if instructions(0x227124, 4) != [
            ('movs', 'r0, #1'), ('strb', 'r0, [r4, #0x11]')]:
        raise ValueError('readiness selector setter differs')
    if instructions(0x28758e, 4) != [('movs', 'r1, #0x27'), ('mvns', 'r6, r1')]:
        raise ValueError('measurement parser does not enumerate forty records')
    if instructions(0x2875b4, 8) != [
            ('ldrb', 'r0, [r5, #7]'), ('ldrb', 'r1, [r5, #6]'),
            ('lsls', 'r1, r1, #8'), ('adds', 'r0, r0, r1')]:
        raise ValueError('measurement ARFCN byte layout differs')
    if instructions(0x2875c0, 6) != [
            ('ldrb', 'r1, [r5, #9]'), ('lsls', 'r0, r1, #0x18'),
            ('asrs', 'r0, r0, #0x18')]:
        raise ValueError('measurement signed RSSI layout differs')
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
