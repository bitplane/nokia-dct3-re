"""Own NPE-3 accessory decision entrance; not electrical calibration."""

import hashlib
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_BIG_ENDIAN


def verify(image):
    if hashlib.sha1(image).hexdigest() != '3d9ea319503e78ec69b60d72cda23e461e118ea9':
        raise ValueError('requires acquired NPE-3 v5.56 PPM C')
    decoder = Cs(CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_BIG_ENDIAN)
    def instructions(start, size):
        return [(item.mnemonic, item.op_str) for item in decoder.disasm(
            image[start - 0x200000:start - 0x200000 + size], start)]
    spans = (
        (0x41a944, 4, [('push', '{r4, lr}'), ('adds', 'r4, r0, #0')]),
        (0x41a950, 20, [('cmp', 'r4, #5'), ('bge', '#0x41a962'),
                       ('ldr', 'r0, [pc, #0x20c]'), ('strb', 'r4, [r0]'),
                       ('adds', 'r0, r4, #0'), ('bl', '#0x41a6b0'),
                       ('bl', '#0x4202a0'), ('movs', 'r0, #1')]),
        (0x41abfc, 2, [('movs', 'r4, #0')]),
        (0x41ac30, 4, [('ldr', 'r0, [pc, #0x370]'), ('strb', 'r4, [r0]')]),
        (0x39d478, 2, [('ldr', 'r6, [pc, #0x358]')]),
        (0x39d522, 4, [('movs', 'r0, #0x84'), ('strb', 'r0, [r6]')]),
        (0x39d560, 4, [('movs', 'r0, #0x80'), ('strb', 'r0, [r6]')]),
        (0x39d584, 4, [('movs', 'r0, #0xf'), ('strb', 'r0, [r6]')]),
        (0x39db6c, 12, [('ldr', 'r0, [pc, #0x1e4]'), ('ldrb', 'r1, [r0]'),
                        ('cmp', 'r1, #0x20'), ('beq', '#0x39db84'),
                        ('movs', 'r1, #0x20'), ('strb', 'r1, [r0]')]),
        (0x46b3d6, 8, [('push', '{r4, lr}'), ('movs', 'r0, #0'), ('bl', '#0x504100')]),
        (0x39d680, 6, [('ldrb', 'r0, [r5]'), ('cmp', 'r0, #0xf'), ('bne', '#0x39d6a8')]),
        (0x39d686, 12, [('bl', '#0x46b3d6'), ('movs', 'r1, #0xff'),
                          ('adds', 'r1, #0xf5'), ('cmp', 'r0, r1'), ('bgt', '#0x39d6a8')]),
        (0x39d692, 12, [('bl', '#0x46b3d6'), ('movs', 'r1, #0xff'),
                          ('adds', 'r1, #0x2d'), ('cmp', 'r0, r1'), ('blt', '#0x39d6a8')]),
        (0x39d6a2, 6, [('lsrs', 'r0, r0, #3'), ('bhs', '#0x39d6a8'), ('b', '#0x39d7d8')]),
        (0x39d6ac, 6, [('cmp', 'r0, #1'), ('bne', '#0x39d6b2'), ('b', '#0x39d7d8')]),
        (0x39d6b2, 20, [('ldrb', 'r2, [r5]'), ('lsrs', 'r0, r2, #7'),
                          ('cmp', 'r0, #0'), ('bne', '#0x39d796'),
                          ('ldrh', 'r0, [r6]'), ('ldr', 'r1, [pc, #0x370]'),
                          ('cmp', 'r0, r1'), ('blt', '#0x39d6c6'), ('bl', '#0x39cc5c')]),
        (0x39cc5c, 12, [('movs', 'r1, #0x39'), ('lsls', 'r1, r1, #4'),
                          ('cmp', 'r0, r1'), ('bge', '#0x39cc68'), ('bl', '#0x39d814')]),
        (0x39ccc4, 10, [('ldr', 'r1, [pc, #0x364]'), ('movs', 'r0, #0x10'),
                          ('strb', 'r0, [r1]'), ('bl', '#0x39d784')]),
    )
    for start, size, expected in spans:
        if instructions(start, size) != expected:
            raise ValueError(f'own accessory instruction span differs at {start:x}')
    literals = {0x39d7d4: 0x173a97, 0x39da0c: 0x173aa2, 0x39da30: 0x312,
                0x41ab64: 0x16ed5c, 0x41afa4: 0x16ed5c, 0x39dd54: 0x173a97}
    for address, value in literals.items():
        if int.from_bytes(image[address - 0x200000:address - 0x200000 + 4], 'big') != value:
            raise ValueError(f'own accessory literal differs at {address:x}')
    setters = {0x39cdba: 0, 0x39d528: 4, 0x39d566: 3, 0x39d58a: 0,
               0x39d786: 1, 0x39db7a: 2, 0x39dc9e: 0, 0x4ba860: 0}
    for address, value in setters.items():
        if instructions(address - 2, 6) != [('movs', f'r0, #{value}'), ('bl', '#0x41a944')]:
            raise ValueError(f'context setter argument differs at {address:x}')
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
        if address + 4 + displacement == 0x41a944:
            calls.append(address)
    if calls != sorted(setters):
        raise ValueError('context setter direct-call inventory differs')
    return {'selector': 0, 'state': 0x173a97, 'sample': 0x173aa2,
            'state_0f_interval': [300, 500], 'decision_threshold': 786,
            'cleanup_threshold': 912, 'high_state_branch': 0x39d796,
            'parameter_context': 0x16ed5c, 'context_setter': 0x41a944,
            'context_initializer': 0x41ac32,
            'context_setter_direct_calls': setters,
            'context_transition_pairs': {0x0f: 0, 0x10: 1, 0x20: 2, 0x80: 3, 0x84: 4},
            'indirect_writer_coverage': False,
            'pcm_semantics_validated': False}
