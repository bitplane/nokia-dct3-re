"""Pinned NPE-3 idle exits; no claim about physical clock/wake semantics."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

BASE = 0x200000
SHA1 = '3d9ea319503e78ec69b60d72cda23e461e118ea9'
BLOCKS = (
    (0x4b03e2, '46487030483870074640732878302800d0bce7fb4658726820f77ae9400872e8'
                 'f04ffb882800d103f01bffea2800d0aaf01cf8472001e7a7'),
    (0x500a18, 'b5101c04f7dbfb712c00d105480b21fe7b4240117341bd10'
                 '480821027b42431122fe400a7342bd10'),
    (0x395e8e, '200146f7'),
)


def decode_blocks(image):
    """Validate complete blocks before interpreting their literals or successors."""
    for address, encoded in BLOCKS:
        expected = bytes.fromhex(encoded)
        if image[address-BASE:address-BASE+len(expected)] != expected:
            raise ValueError(f'idle control-flow block differs at {address:#x}')
    for address in (0x500a24, 0x500a30):
        opcode = struct.unpack_from('>H', image, address-BASE)[0]
        literal = ((address + 4) & ~3) + (opcode & 255) * 4
        if struct.unpack_from('>I', image, literal-BASE)[0] != 0x20000:
            raise ValueError('clock-helper MMIO base differs')
    return {
        'explicit_enable': '0x4b03ec', 'enable_value': 5,
        'guard_return': '0x4b040e', 'zero_guard_target': '0x4b0368',
        'nonzero_guard_target': '0x4b0412', 'clock_helper': '0x500a18',
        'clock_store_targets': ['0x2000d', '0x2000d'],
        'both_helper_arms_clear_bit0': True,
        'nonzero_argument_sets_bit1': True,
        'helper_restores_global_delivery': False,
        'all_idle_exit_traverses_explicit_enable': False,
        'final_idle_predicate_constant': 1,
        'physical_clock_semantics_established': False,
    }


def inspect(image):
    if hashlib.sha1(image).hexdigest() != SHA1:
        raise ValueError('requires unchanged acquired NPE-3 v5.56 PPM C')
    return decode_blocks(image)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('flash', type=Path)
    args = parser.parse_args()
    try:
        result = inspect(args.flash.read_bytes())
    except (OSError, ValueError) as error:
        parser.exit(1, f'6210 idle contract FAIL: {error}\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
