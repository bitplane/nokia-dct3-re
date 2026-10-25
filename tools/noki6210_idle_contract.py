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


def conditional_target(address,opcode):
    """Thumb-1 conditional branch, including backward signed displacements."""
    if opcode & 0xf000 != 0xd000 or (opcode >> 8) & 15 >= 14:
        raise ValueError('requires Thumb conditional branch')
    displacement=opcode & 255
    if displacement & 128:
        displacement-=256
    return address+4+2*displacement


def enable_predecessors(image):
    def halfword(address):
        return struct.unpack_from('>H',image,address-BASE)[0]
    # R5 is the own MMIO base; R8/R10 carry the explicit enable/disable values.
    expected={0x4b0340:0xb5f0,0x4b0354:0x2005,0x4b0356:0x4680,
              0x4b035e:0x200a,0x4b0360:0x4682,0x4b0362:0x4d3e,
              0x4b03a6:0x4650,0x4b03a8:0x7328,0x4b03ea:0x4640,0x4b03ec:0x7328}
    if any(halfword(a)!=word for a,word in expected.items()):
        raise ValueError('idle control register/value provenance differs')
    literal=((0x4b0362+4)&~3)+(halfword(0x4b0362)&255)*4
    if struct.unpack_from('>I',image,literal-BASE)[0]!=0x20000:
        raise ValueError('idle control store MMIO base differs')
    branches=[(0x4b03b0,0xd017),(0x4b03b8,0xd11d),(0x4b03c0,0xd00f),
              (0x4b03c8,0xd00b),(0x4b03d0,0xd007),(0x4b03d8,0xd003),(0x4b03e0,0xd109)]
    if any(halfword(a)!=word for a,word in branches):
        raise ValueError('idle permission branch differs')
    direct=[hex(a) for a in range(0x4b0340,0x4b041a,2) if halfword(a)&0xfff8==0x7328]
    if direct!=['0x4b03a8','0x4b03ec']:
        raise ValueError('idle direct control-store inventory differs')
    return dict(direct_control_stores=direct,
        enable_predecessors=[hex(a) for a,word in branches if conditional_target(a,word)==0x4b03e2],
        other_permission_targets=[hex(conditional_target(a,word)) for a,word in branches
                                  if conditional_target(a,word)!=0x4b03e2],
        scope='R5-relative byte stores at offset12 and seven permission branches; excludes aliases and transitive helper writers')


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
        'permission_branch_inventory':enable_predecessors(image),
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
