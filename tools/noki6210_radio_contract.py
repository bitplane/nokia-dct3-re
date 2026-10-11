"""Pin NPE-3's own acquisition RX routing and channel correlation."""
import hashlib
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_BIG_ENDIAN


def verify(image):
    if hashlib.sha1(image).hexdigest() != '3d9ea319503e78ec69b60d72cda23e461e118ea9':
        raise ValueError('requires acquired NPE-3 v5.56 PPM C')
    def read(address, size):
        return image[address - 0x200000:address - 0x200000 + size]
    table = [int.from_bytes(read(0x4f6078 + 4*i, 4), 'big') for i in range(13)]
    if table != [0x4f6114, 0x4f610c, 0x4f612e, 0x4f6104, 0x4f60fc,
                 0x4f60f4, 0x4f60ec, 0x4f60e4, 0x4f60dc, 0x4f60d4,
                 0x4f612e, 0x4f612e, 0x4f60cc]:
        raise ValueError('own thirteen-entry DSP RX table differs')
    decoder = Cs(CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_BIG_ENDIAN)
    expected = {
        0x4f611e: ('bl', '#0x457f80'),
        0x457fc0: ('beq', '#0x458014'),
        0x458018: ('bl', '#0x457f28'),
        0x457f60: ('movs', 'r0, #1'),
        0x457f62: ('lsls', 'r0, r0, #0xb'),
        0x457f64: ('strh', 'r0, [r4]'),
        0x457f6c: ('movs', 'r0, #0x12'),
        0x457f70: ('bl', '#0x3c22c0'),
        0x3b930a: ('ldrsh', 'r4, [r5, r0]'),
        0x3b9342: ('str', 'r5, [r2, #0x1c]'),
        0x3ba708: ('bl', '#0x3b9302'),
        0x3ba734: ('lsls', 'r1, r1, #0xb'),
        0x3ba736: ('cmp', 'r0, r1'),
        0x3ba73c: ('cmp', 'r1, #0x50'),
        0x3ba75e: ('bl', '#0x3b9e34'),
        0x3b9e34: ('push', '{r4, r5, r6, r7, lr}'),
        0x3ba0c0: ('adds', 'r1, r5, #3'),
        0x3ba0c4: ('bl', '#0x3b9388'),
        0x3ba0d0: ('movs', 'r0, #0x19'),
        0x3ba0d2: ('bl', '#0x3b9598'),
        0x3b9604: ('b', '#0x3b9a4c'),
        0x3b9a6e: ('bl', '#0x43f074'),
        0x3b9a72: ('cmp', 'r0, #1'),
        0x3b9a74: ('beq', '#0x3b9a82'),
        0x3b9a82: ('movs', 'r6, #0xff'),
        0x3ba0d6: ('cmp', 'r0, #1'),
        0x4f6052: ('movs', 'r0, #0x80'),
        0x4f605a: ('subs', 'r0, #3'),
        0x4f605c: ('cmp', 'r0, #0xc'),
        0x4f60de: ('bl', '#0x45835c'),
        0x458370: ('movs', 'r0, #0xe'),
        0x458372: ('bl', '#0x3c22c0'),
        0x4f60ee: ('bl', '#0x4580e8'),
        0x458106: ('ldrb', 'r1, [r4, #4]'),
        0x458108: ('lsls', 'r1, r1, #0x1f'),
        0x45810a: ('lsrs', 'r1, r1, #0x1f'),
        0x45810c: ('ldrb', 'r2, [r0, #2]'),
        0x45810e: ('cmp', 'r1, r2'),
        0x458110: ('bne', '#0x45815e'),
    }
    for address, value in expected.items():
        ins = next(decoder.disasm(read(address, 4), address), None)
        if ins is None or (ins.mnemonic, ins.op_str) != value:
            raise ValueError(f'own acquisition contract differs at {address:x}')
    return {'rx_table': '4f6078', 'type_8b_handler': '45835c',
            'bcch_status': '0800', 'bcch_destination_task': 18,
            'bcch_object_consumer': '3b9302', 'si3_parser': '43f074',
            'type_8b_destination_task': 14, 'type_89_handler': '4580e8',
            'correlation': 'body bit 0 equals pending context byte 2',
            'traffic_release': 'runtime physical End observation required'}
