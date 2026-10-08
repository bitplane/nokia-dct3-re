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
    promotion_states = [int.from_bytes(read(0x2a139c + state * 4, 4), 'big')
                        for state in (4, 5, 6)]
    if promotion_states != [0x2a1468, 0x2a1456, 0x2a1444]:
        raise ValueError('queue-promotion selector states differ')
    for address, alternate in ((0x2a1468, 0x2a14cc),
                               (0x2a1456, 0x2a1464),
                               (0x2a1444, 0x2a1452)):
        if instructions(address, 4) != [('cmp', 'r6, #0'), ('bne', f'#{alternate:#x}')]:
            raise ValueError('queue promotion is not argument-zero owned')
    if int.from_bytes(read(0x2a17c4, 4), 'big') != 0x13721e:
        raise ValueError('queue-promotion enable byte differs')
    if instructions(0x2a1474, 10) != [
            ('ldr', 'r0, [pc, #0x34c]'), ('ldrb', 'r0, [r0]'),
            ('cmp', 'r0, #0'), ('bne', '#0x2a147e'), ('b', '#0x2a1934')]:
        raise ValueError('queue-promotion enable predicate differs')
    if instructions(0x2a1484, 8) != [
            ('bl', '#0x2a0dc8'), ('ldr', 'r1, [r4, #4]'), ('strb', 'r0, [r1, #3]')]:
        raise ValueError('queue-promotion call/return ownership differs')
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
    if instructions(0x258c1a, 10) != [
            ('movs', 'r0, #0x7f'), ('lsls', 'r1, r0, #4'),
            ('ldr', 'r0, [sp, #8]'), ('cmp', 'r0, r1'),
            ('beq', '#0x258c3a')]:
        raise ValueError('07f0 prerequisite input selector differs')
    if int.from_bytes(read(0x258fd4, 4), 'big') != 0x9fc:
        raise ValueError('07f0 prerequisite producer literal differs')
    if instructions(0x258c6e, 4) != [('bl', '#0x2af30c')]:
        raise ValueError('09fc prerequisite constructor boundary differs')
    if [int.from_bytes(read(address, 4), 'big') for address in
            (0x2a2218, 0x2a2274)] != [0x137f58, 0x3ea]:
        raise ValueError('selected-cell status dispatcher context/origin differs')
    if instructions(0x2a21b6, 6) != [
            ('subs', 'r0, #1'), ('cmp', 'r0, #0'), ('beq', '#0x2a21fa')]:
        raise ValueError('03eb selected-cell publication selection differs')
    if instructions(0x2a21fa, 6) != [
            ('movs', 'r0, #0x7f'), ('lsls', 'r0, r0, #4'),
            ('b', '#0x2a21f4')]:
        raise ValueError('03eb to 07f0 status construction differs')
    if instructions(0x21bffa, 4) != [('bl', '#0x2a21a4')]:
        raise ValueError('observed selected-cell publication caller differs')
    if int.from_bytes(read(0x2a110c, 4), 'big') != 0x137f58:
        raise ValueError('selected-cell request queue context differs')
    if instructions(0x2a0dcc, 10) != [
            ('ldr', 'r0, [r1, #0xc]'), ('cmp', 'r0, #0'),
            ('bne', '#0x2a0dd6'), ('movs', 'r4, #0'), ('b', '#0x2a0dfa')]:
        raise ValueError('empty replacement request handling differs')
    if instructions(0x2a0df0, 10) != [
            ('ldr', 'r0, [r1, #0xc]'), ('movs', 'r4, #2'),
            ('str', 'r0, [r1, #8]'), ('movs', 'r0, #0'), ('str', 'r0, [r1, #0xc]')]:
        raise ValueError('replacement request promotion differs')
    if instructions(0x21f5aa, 6) != [
            ('ldr', 'r0, [r4, #8]'), ('bl', '#0x2a1eaa')]:
        raise ValueError('late readiness request queue insertion differs')
    if instructions(0x2a1ec0, 4) != [
            ('ldrsh', 'r1, [r5, r1]'), ('strh', 'r1, [r0]')]:
        raise ValueError('queued request input copy differs')
    if instructions(0x21f5d6, 8) != [
            ('cmp', 'r0, #3'), ('beq', '#0x21f5de'), ('bl', '#0x21b8ea')]:
        raise ValueError('late readiness result continuation differs')
    if instructions(0x21b8ea, 8) != [
            ('adr', 'r0, #0x2d4'), ('bl', '#0x2d5dcc'), ('b', '#0x21bb8c')]:
        raise ValueError('PH9000 alternate does not return directly to receive loop')
    if instructions(0x21bb92, 10) != [
            ('ldr', 'r0, [r4, #8]'), ('bl', '#0x288e44'), ('bl', '#0x2886b0')]:
        raise ValueError('PH9000 message disposal and receive boundary differs')
    if int.from_bytes(read(0x21bbe4 + 13 * 4, 4), 'big') != 0x21ef30:
        raise ValueError('state-13 measurement recovery dispatch differs')
    if instructions(0x21ef3c, 8) != [
            ('ldrb', 'r0, [r4]'), ('cmp', 'r0, #0x8b'),
            ('beq', '#0x21ef44'), ('b', '#0x21ebc4')]:
        raise ValueError('state-13 measurement completion selector differs')
    if instructions(0x21ef4a, 10) != [
            ('ldr', 'r0, [r4, #8]'), ('bl', '#0x2a2250'), ('bl', '#0x21f900')]:
        raise ValueError('state-13 measurement recovery does not reevaluate queued request')
    if instructions(0x2b2ff2, 12) != [
            ('movs', 'r0, #0xa0'), ('strb', 'r0, [r4, #2]'),
            ('movs', 'r0, #2'), ('strh', 'r0, [r4]'),
            ('movs', 'r0, #0x56'), ('strb', 'r0, [r4, #3]')]:
        raise ValueError('own candidate-list packet header differs')
    if instructions(0x286c50, 6) != [('bl', '#0x2b2fe0'), ('adds', 'r7, r0, #0')]:
        raise ValueError('candidate-list producer constructor differs')
    if instructions(0x286c74, 8) != [
            ('adds', 'r0, r7, #0'), ('bl', '#0x2b300e'),
            ('pop', '{r4, r5, r6, r7, pc}')]:
        raise ValueError('candidate-list producer send tail differs')
    if instructions(0x21faaa, 4) != [('bl', '#0x286c48')]:
        raise ValueError('candidate-window producer caller differs')
    if instructions(0x21fb78, 6) != [
            ('cmp', 'r2, #0x8b'), ('beq', '#0x21fb7e'), ('b', '#0x21f44e')]:
        raise ValueError('candidate-window measurement receive selector differs')
    if instructions(0x21fb84, 6) != [('ldr', 'r0, [r4, #8]'), ('bl', '#0x2a2250')]:
        raise ValueError('candidate-window measurement parser differs')
    if instructions(0x21fbb0, 6) != [
            ('cmp', 'r0, #0'), ('bne', '#0x21fbb6'), ('b', '#0x21fa68')]:
        raise ValueError('candidate-window zero-result retry differs')
    if instructions(0x2b2dc8, 12) != [
            ('movs', 'r0, #4'), ('strb', 'r0, [r4, #2]'),
            ('movs', 'r0, #2'), ('strh', 'r0, [r4]'),
            ('movs', 'r0, #0x55'), ('strb', 'r0, [r4, #3]')]:
        raise ValueError('own scan-control packet header differs')
    if instructions(0x2b2ddc, 8) != [
            ('adds', 'r0, r6, #0'), ('adds', 'r1, r5, #0'), ('bl', '#0x2b2d52')]:
        raise ValueError('scan-control selector mapping differs')
    if int.from_bytes(read(0x2b30d0, 4), 'big') != 0x33e8a9 or read(0x33e8a9, 3) != bytes.fromhex('050303'):
        raise ValueError('selector-three scan-control options differ')
    for address in (0x21f3c4, 0x21fa3a):
        expected_branch = '#0x21f3d4' if address == 0x21f3c4 else '#0x21fa4e'
        if instructions(address, 8) != [
                ('ldrb', 'r1, [r0, #9]'), ('cmp', 'r1, #0'),
                ('beq', expected_branch), ('movs', 'r1, #2')]:
            raise ValueError('scan-control option request ownership differs')
    if instructions(0x2b31c6, 4) != [('movs', 'r0, #0x57'), ('strb', 'r0, [r4, #3]')]:
        raise ValueError('background measurement packet type differs')
    if instructions(0x2b31d2, 8) != [
            ('adds', 'r0, r6, #0'), ('adds', 'r1, r5, #0'), ('bl', '#0x2b2d52')]:
        raise ValueError('background measurement does not share selector mapping')
    # Enumerate aligned direct-BL candidates, not indirect-call ownership.
    parser_calls = []
    selector_calls = []
    wrapper_calls = []
    alternate_readiness_calls = []
    state_calls = []
    for offset in range(0, len(image) - 4, 2):
        if image[offset] & 0xf8 != 0xf0:
            continue
        candidate = instructions(0x200000 + offset, 4)
        if candidate == [('bl', '#0x2a2250')]:
            parser_calls.append(0x200000 + offset)
        if candidate == [('bl', '#0x2a1380')]:
            selector_calls.append(0x200000 + offset)
        if candidate == [('bl', '#0x21f900')]:
            wrapper_calls.append(0x200000 + offset)
        if candidate == [('bl', '#0x21f2ec')]:
            alternate_readiness_calls.append(0x200000 + offset)
        if candidate == [('bl', '#0x21bdc4')]:
            predecessor = instructions(0x200000 + offset - 2, 2)
            state_calls.append((0x200000 + offset, predecessor))
    if parser_calls != [0x21ef4c, 0x21fb86]:
        raise ValueError('direct measurement-parser candidate callsites differ')
    if selector_calls != [0x21d886, 0x21f356, 0x21f5ca, 0x21f90a,
                          0x21fba2, 0x2a1e28, 0x2a1e62, 0x2a224a]:
        raise ValueError('direct selector candidate callsites differ')
    if wrapper_calls != [0x21ef50]:
        raise ValueError('direct selector-wrapper candidate callsites differ')
    if alternate_readiness_calls != [0x21fc4a]:
        raise ValueError('alternate readiness direct entrance candidates differ')
    if any(int.from_bytes(image[offset:offset + 4], 'big') in
           (0x21f2ec, 0x21f2ed) for offset in range(0, len(image) - 3, 4)):
        raise ValueError('alternate readiness literal pointer candidate present')
    expected_states = [7, 5, 26, 4, 6, 2, 23, 24, 25, 11, 13, 15,
                       17, 12, 9, 10, 21, 28, 20]
    if len(state_calls) != len(expected_states):
        raise ValueError('direct receive-state setter candidate count differs')
    for (_, predecessor), state in zip(state_calls, expected_states):
        operand = f'r0, #{state:#x}' if state >= 10 else f'r0, #{state}'
        if predecessor != [('movs', operand)]:
            raise ValueError('direct receive-state setter argument differs')
    for address, state in ((0x21f39e, 22), (0x21f424, 19),
                           (0x21f45c, 14), (0x21f46a, 16)):
        if instructions(address, 4) != [
                ('movs', f'r0, #{state:#x}'), ('b', '#0x21f482')]:
            raise ValueError('inline receive-state continuation differs')
    if instructions(0x21f480, 8) != [
            ('movs', 'r0, #0x12'), ('strh', 'r0, [r4, #2]'),
            ('ldr', 'r4, [pc, #0x36c]'), ('movs', 'r0, #4')]:
        raise ValueError('inline state-eighteen continuation differs')
    if instructions(0x21bdc2, 4) != [
            ('movs', 'r0, #1'), ('strh', 'r0, [r4, #2]')]:
        raise ValueError('inline state-one entrance differs')
    # Tail branches are a separate entrance class; a BL census cannot close it.
    if instructions(0x21f2c0, 2) != [('b', '#0x21f900')]:
        raise ValueError('state-ten acknowledgement tail entrance differs')
    if instructions(0x21f2ae, 6) != [
            ('ldrb', 'r0, [r4]'), ('cmp', 'r0, #0x89'), ('bne', '#0x21f2c2')]:
        raise ValueError('state-ten acknowledgement class predicate differs')
    if int.from_bytes(read(0x21bbe4 + 21 * 4, 4), 'big') != 0x21fc40:
        raise ValueError('state-21 readiness dispatcher differs')
    if instructions(0x21fc44, 10) != [
            ('ldr', 'r0, [pc, #0x3a4]'), ('cmp', 'r1, r0'),
            ('beq', '#0x21fc4e'), ('bl', '#0x21f2ec')]:
        raise ValueError('state-21 alternate readiness entrance differs')
    if instructions(0x21f2f6, 30) != [
            ('ldr', 'r0, [pc, #0x230]'), ('cmp', 'r1, r0'),
            ('beq', '#0x21f314'), ('movs', 'r0, #0xfb'),
            ('lsls', 'r0, r0, #2'), ('cmp', 'r1, r0'),
            ('beq', '#0x21f314'), ('ldr', 'r0, [pc, #0x23c]'),
            ('cmp', 'r1, r0'), ('beq', '#0x21f314'),
            ('movs', 'r0, #0x15'), ('bl', '#0x21bdc4'),
            ('movs', 'r3, r2'), ('ldrb', 'r0, [r5, #5]')]:
        raise ValueError('alternate readiness unmatched-input state transition differs')
    for pool, expected in ((0x21f434, 0x03e9), (0x21f528, 0x03eb),
                           (0x21f544, 0x03ea)):
        if int.from_bytes(read(pool, 4), 'big') != expected:
            raise ValueError('alternate readiness input literal differs')
    if instructions(0x21f354, 6) != [('movs', 'r0, #0'), ('bl', '#0x2a1380')]:
        raise ValueError('alternate readiness is not argument-zero owned')
    if instructions(0x21f552, 2) != [('b', '#0x21f5aa')]:
        raise ValueError('shared forwarded readiness tail differs')
    if int.from_bytes(read(0x21bedc, 4), 'big') != 0x137db0:
        raise ValueError('generic measurement flag address differs')
    if instructions(0x21bb5a, 6) != [
            ('ldr', 'r1, [pc, #0x380]'), ('movs', 'r0, #1'), ('b', '#0x21b950')]:
        raise ValueError('generic measurement flag setter differs')
    if instructions(0x2865dc, 4) != [('cmp', 'r0, #1'), ('bne', '#0x2865f2')]:
        raise ValueError('measurement flag consumer enable predicate differs')
    if int.from_bytes(read(0x28693c, 4), 'big') != 0x13722c:
        raise ValueError('measurement flag normalization outcome root differs')
    if instructions(0x2865e4, 16) != [
            ('ldr', 'r0, [r1]'), ('cmp', 'r0, #1'), ('beq', '#0x2865ee'),
            ('cmp', 'r0, #2'), ('bne', '#0x2865f2'),
            ('movs', 'r0, #0'), ('str', 'r0, [r1]'), ('mov', 'pc, lr')]:
        raise ValueError('measurement flag outcome normalization differs')
    if instructions(0x287606, 8) != [
            ('movs', 'r1, #0x68'), ('mov', 'r0, fp'),
            ('cmn', 'r1, r0'), ('bmi', '#0x287638')]:
        raise ValueError('measurement terminal signed RSSI predicate differs')
    if instructions(0x287634, 16) != [
            ('movs', 'r0, #4'), ('b', '#0x287640'),
            ('mov', 'r0, sl'), ('cmp', 'r0, #0'), ('beq', '#0x287634'),
            ('movs', 'r0, #3'), ('ldr', 'r5, [pc, #0x104]'), ('str', 'r0, [r4]')]:
        raise ValueError('measurement terminal skipped-entry outcome differs')
    if int.from_bytes(read(0x28651c, 4), 'big') != 0x137248:
        raise ValueError('flag-record table address differs')
    if instructions(0x28621a, 4) != [('lsls', 'r2, r1, #3'), ('adds', 'r1, r1, r2')]:
        raise ValueError('flag-record table stride differs')
    if instructions(0x2a117a, 20) != [
            ('movs', 'r4, #3'), ('lsls', 'r0, r4, #0x18'),
            ('lsrs', 'r1, r0, #0x18'), ('adds', 'r0, r5, #0'),
            ('bl', '#0x286218'), ('adds', 'r5, #0xc'),
            ('adds', 'r4, #1'), ('cmp', 'r4, #7'), ('blt', '#0x2a117c')]:
        raise ValueError('complementary flag-record bank indices differ')
    if instructions(0x2a15b2, 8) != [
            ('movs', 'r1, #0xfb'), ('lsls', 'r1, r1, #2'),
            ('cmp', 'r0, r1'), ('beq', '#0x2a168e')]:
        raise ValueError('state-two 03ec record-bank selection differs')
    if instructions(0x2a1694, 4) != [('bl', '#0x2a1170')]:
        raise ValueError('state-two complementary bank consumer differs')
    promotion_calls = []
    for offset in range(0, len(image) - 4, 2):
        if image[offset] & 0xf8 == 0xf0 and instructions(0x200000 + offset, 4) == [('bl', '#0x2a0dc8')]:
            promotion_calls.append(0x200000 + offset)
    if promotion_calls != [0x2a13d0, 0x2a1484, 0x2a193a, 0x2a1988]:
        raise ValueError('direct queue-promotion candidate callsites differ')
    if instructions(0x2a187a, 8) != [
            ('cmp', 'r6, #2'), ('beq', '#0x2a197c'),
            ('cmp', 'r6, #3'), ('beq', '#0x2a18bc')]:
        raise ValueError('selector argument-two promotion dispatch differs')
    if instructions(0x21d884, 6) != [('movs', 'r0, #2'), ('bl', '#0x2a1380')]:
        raise ValueError('explicit queue-promotion selector caller differs')
    if int.from_bytes(read(0x220198, 4), 'big') != 0x413:
        raise ValueError('explicit queue-promotion event selector differs')
    if instructions(0x21fe00, 8) != [
            ('bl', '#0x287b38'), ('cmp', 'r0, #0'),
            ('bne', '#0x21fe0c')]:
        raise ValueError('timer remaining-duration recovery guard differs')
    if [int.from_bytes(read(address, 4), 'big') for address in
            (0x287bd0, 0x287bd4)] != [0x11174c, 0x1115d0]:
        raise ValueError('timer descriptor and clock roots differ')
    if instructions(0x287b42, 18) != [
            ('movs', 'r0, #0xc'), ('muls', 'r0, r4, r0'),
            ('ldr', 'r1, [pc, #0x88]'), ('adds', 'r2, r1, r0'),
            ('ldrb', 'r0, [r2, #8]'), ('cmp', 'r0, #2'),
            ('beq', '#0x287b56'), ('ldrh', 'r1, [r2, #4]'),
            ('ldr', 'r5, [pc, #0x80]')]:
        raise ValueError('timer descriptor stride/state/duration reads differ')
    if instructions(0x287c0a, 8) != [
            ('mov', 'r0, sp'), ('ldrh', 'r0, [r0]'),
            ('add', 'sp, #4'), ('pop', '{r4, r5, r6, pc}')]:
        raise ValueError('timer query halfword return differs')
    for address, literal_offset in ((0x21bca2, '#0x370'), (0x21bcf2, '#0x320')):
        if instructions(address, 8) != [
                ('movs', 'r0, #0x81'), ('ldr', f'r1, [pc, {literal_offset}]'),
                ('bl', '#0x2879fe')]:
            raise ValueError('explicit timer-81 setup candidate differs')
    if int.from_bytes(read(0x21c018, 4), 'big') != 0x75a:
        raise ValueError('timer-81 setup duration differs')
    if [int.from_bytes(read(0x21bbe4 + 4 * state, 4), 'big')
            for state in (23, 24)] != [0x21e93a, 0x21e786]:
        raise ValueError('timer setup receive-state entries differ')
    if instructions(0x21e944, 10) != [
            ('ldrb', 'r0, [r4]'), ('cmp', 'r0, #0x89'),
            ('bne', '#0x21e94e'), ('bl', '#0x21bc88')]:
        raise ValueError('state-23 type-89 timer setup route differs')
    if instructions(0x21e906, 4) != [('bl', '#0x21bcec')]:
        raise ValueError('state-24 timer setup caller differs')
    if int.from_bytes(read(0x21bbd4, 4), 'big') != 0x138038:
        raise ValueError('task-12 receive-state context differs')
    if instructions(0x21bdc4, 6) != [
            ('strh', 'r0, [r4, #2]'), ('bl', '#0x21f48a')]:
        raise ValueError('task-12 state-store continuation differs')
    if int.from_bytes(read(0x21bbe4 + 4 * 15, 4), 'big') != 0x21f8e2:
        raise ValueError('state-15 acknowledgement handler differs')
    if int.from_bytes(read(0x21f9a0, 4), 'big') != 0x1802:
        raise ValueError('state-15 DSP message selector differs')
    if instructions(0x21f8f0, 16) != [
            ('ldrb', 'r0, [r4]'), ('cmp', 'r0, #0x89'),
            ('beq', '#0x21f8fa'), ('bl', '#0x21ef54'),
            ('adr', 'r0, #0x36c'), ('bl', '#0x2d5dcc')]:
        raise ValueError('state-15 type-89 acknowledgement route differs')
    if int.from_bytes(read(0x21f188, 4), 'big') != 0x411:
        raise ValueError('state-13 alternate input selector differs')
    if instructions(0x21ee30, 12) != [
            ('ldr', 'r0, [pc, #0x354]'), ('cmp', 'r1, r0'),
            ('beq', '#0x21ee4c'), ('movs', 'r0, #0xd'),
            ('bl', '#0x21bdc4')]:
        raise ValueError('state-13 default continuation differs')
    if instructions(0x21ee52, 14) != [
            ('ldr', 'r0, [pc, #0x64]'), ('ldr', 'r0, [r0]'),
            ('ldr', 'r0, [r0]'), ('lsrs', 'r1, r0, #1'),
            ('beq', '#0x21eef2'), ('cmp', 'r0, #2'), ('beq', '#0x21eef2')]:
        raise ValueError('state-13 special-input outcome predicate differs')
    if instructions(0x21ee7a, 14) != [
            ('bl', '#0x287272'), ('ldr', 'r5, [pc, #0x1b8]'),
            ('ldr', 'r0, [r5]'), ('strb', 'r6, [r0, #2]'), ('bl', '#0x21fb8c')]:
        raise ValueError('state-13 special-input argument-zero continuation differs')
    if int.from_bytes(read(0x2e1150, 4), 'big') != 0x305ec5:
        raise ValueError('special-status transport callback literal differs')
    if instructions(0x2e0fae, 6) != [
            ('ldr', 'r0, [pc, #0x1a0]'), ('bl', '#0x2b98e4')]:
        raise ValueError('special-status transport callback registration differs')
    if int.from_bytes(read(0x2b9c80, 4), 'big') != 0x12bab4:
        raise ValueError('transport callback root differs')
    if instructions(0x2b98e4, 6) != [
            ('ldr', 'r1, [pc, #0x398]'), ('str', 'r0, [r1]'), ('mov', 'pc, lr')]:
        raise ValueError('transport callback registrar differs')
    if instructions(0x2ba074, 6) != [
            ('adds', 'r1, r4, #0'), ('mov', 'lr, pc'), ('bx', 'r3')]:
        raise ValueError('transport packet callback invocation differs')
    if instructions(0x306fbc, 12) != [
            ('subs', 'r0, #0x17'), ('cmp', 'r0, #0'),
            ('beq', '#0x307010'), ('subs', 'r0, #2'),
            ('cmp', 'r0, #0'), ('beq', '#0x307008')]:
        raise ValueError('type-9c RX dispatcher arithmetic differs')
    if instructions(0x307008, 8) != [
            ('adds', 'r0, r4, #0'), ('bl', '#0x2ba010'), ('b', '#0x3070d0')]:
        raise ValueError('type-9c transport callback worker route differs')
    default_selectors = (0x21bcdc, 0x21bce4, 0x21bd24,
                         0x21bd28, 0x21bd2c, 0x21bd7c)
    if [int.from_bytes(read(address, 4), 'big') for address in default_selectors] != [
            0x3fd, 0x3fb, 0x3fa, 0x422, 0x41d, 0x421]:
        raise ValueError('default receive-handler selectors differ')
    if instructions(0x21b9e8, 8) != [
            ('ldr', 'r0, [pc, #0x390]'), ('cmp', 'r1, r0'),
            ('beq', '#0x21b9f0'), ('b', '#0x21b8ea')]:
        raise ValueError('default receive-handler unmatched tail differs')
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
