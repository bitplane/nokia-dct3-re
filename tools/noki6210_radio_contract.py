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
            'type_8b_destination_task': 14, 'type_89_handler': '4580e8',
            'correlation': 'body bit 0 equals pending context byte 2',
            'traffic_release': 'runtime physical End observation required'}
