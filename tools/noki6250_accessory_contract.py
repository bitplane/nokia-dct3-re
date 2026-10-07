"""Validate NHM-3's own selector-0 accessory decision, not its electrical units."""
import hashlib

from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_BIG_ENDIAN


def verify(image):
    if hashlib.sha1(image).hexdigest() != '95607ce39c383bda75f1e6aeae67a214b787b0a1':
        raise ValueError('requires acquired NHM-3 v5.03 MCU/PPM')
    decoder = Cs(CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_BIG_ENDIAN)
    expected = {
        0x4ae340: ('movs', 'r0, #0'),
        0x4ae342: ('bl', '#0x511ee0'),
        0x3b2790: ('bl', '#0x4ae33e'),
        0x3b2794: ('strh', 'r0, [r5]'),
        0x3b2796: ('ldr', 'r6, [pc, #0x1a0]'),
        0x3b279a: ('cmp', 'r0, #0xf'),
        0x3b27ca: ('ldrb', 'r2, [r6]'),
        0x3b27d2: ('ldrh', 'r0, [r5]'),
        0x3b27d4: ('ldr', 'r1, [pc, #0x370]'),
        0x3b27d8: ('blt', '#0x3b27de'),
        0x3b1e4c: ('movs', 'r0, #0x10'),
        0x3b1e4e: ('strb', 'r0, [r1]'),
        0x3b1e50: ('bl', '#0x3b28aa'),
        0x3b28aa: ('movs', 'r0, #1'),
        0x3b28ac: ('bl', '#0x40fde2'),
    }
    for address, value in expected.items():
        ins = next(decoder.disasm(image[address - 0x200000:address - 0x200000 + 4], address), None)
        if ins is None or (ins.mnemonic, ins.op_str) != value:
            raise ValueError(f'own accessory contract differs at {address:x}')
    def word(address):
        return int.from_bytes(image[address - 0x200000:address - 0x200000 + 4], 'big')
    if (word(0x3b2938) != 0x172d70 or word(0x3b219c) != 0x172d70 or
            word(0x4ae6b0) != 0x172cb4 or word(0x3b2b48) != 0x312):
        raise ValueError('accessory state/threshold literal differs')
    return {'selector': 0, 'reader': '4ae33e', 'decision': '3b27ca',
            'state_address': '172d70', 'high_threshold': 0x312,
            'headset_state': 0x10, 'ui_publisher': '40fde2',
            'electrical_unattached_level': 'VBB pull-up; raw scale unmeasured'}
