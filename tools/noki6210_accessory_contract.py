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
    literals = {0x39d7d4: 0x173a97, 0x39da0c: 0x173aa2, 0x39da30: 0x312}
    for address, value in literals.items():
        if int.from_bytes(image[address - 0x200000:address - 0x200000 + 4], 'big') != value:
            raise ValueError(f'own accessory literal differs at {address:x}')
    return {'selector': 0, 'state': 0x173a97, 'sample': 0x173aa2,
            'state_0f_interval': [300, 500], 'decision_threshold': 786,
            'cleanup_threshold': 912, 'high_state_branch': 0x39d796}
