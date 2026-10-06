"""Pinned NPE-3 upload descriptors and acquired PMM journal assessment."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_BIG_ENDIAN

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.extract_nsm3_verifier import extract
from tools.nse5_pmm_journal import replay
from tools.noki6250_pmm_check import checksum


def assess(flash, pmm):
    extract(flash, '6210')  # Verifier provenance and complete program extent.
    decoder = Cs(CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_BIG_ENDIAN)
    instructions = {}
    for low, high in ((0x305a0c, 0x305a64), (0x3029ee, 0x302ab4),
                      (0x3d71f8, 0x3d7230), (0x3d72a6, 0x3d72cc)):
        instructions.update((x.address, (x.mnemonic, x.op_str))
                            for x in decoder.disasm(flash[low-0x200000:high-0x200000], low))
    expected = {
        0x305a0c: ('movs', 'r1, #0x74'),
        0x305a12: ('beq', '#0x305a56'),
        0x305a5e: ('bl', '#0x3029d4'),
        0x3029ee: ('ldrb', 'r1, [r4, #8]'),
        0x3029f6: ('movs', 'r0, #0xa'),
        0x3029fe: ('subs', 'r0, #3'),
        0x302a02: ('beq', '#0x302a52'),
        0x302a56: ('lsrs', 'r0, r0, #3'),
        0x302a58: ('blo', '#0x302b10'),
        0x302a5a: ('movs', 'r0, #0x1b'),
        0x302a86: ('ldrb', 'r0, [r4, #9]'),
        0x302a8e: ('strb', 'r6, [r1, #0x10]'),
        0x302aa4: ('strb', 'r6, [r1, #0x11]'),
        0x3d71fe: ('movs', 'r0, #7'),
        0x3d7200: ('bl', '#0x4f3962'),
        0x3d720c: ('ldr', 'r1, [r2, #0x34]'),
        0x3d7212: ('ldr', 'r1, [r2, #0x38]'),
        0x3d7224: ('movs', 'r1, #0xe8'),
        0x3d72aa: ('movs', 'r2, #0xe1'),
        0x3d72ac: ('lsls', 'r2, r2, #3'),
        0x3d72b0: ('blt', '#0x3d72ca'),
        0x3d72b6: ('bgt', '#0x3d72ca'),
    }
    for address, value in expected.items():
        if instructions.get(address) != value:
            raise ValueError(f'own contract instruction mismatch at {address:x}')
    for address, value in ((0x302dd8, 0x17fd99), (0x302b20, 0x17fbe0),
                           (0x3d74ac, 0x17fcac), (0x3d74b0, 1500),
                           (0x3d7650, 5500)):
        if int.from_bytes(flash[address-0x200000:address-0x200000+4], 'big') != value:
            raise ValueError(f'own contract literal mismatch at {address:x}')
    if flash[0x869dc + 7] != 2:
        raise ValueError('source 7 no longer maps to physical selector 2')
    if hashlib.sha1(pmm).hexdigest() != 'b3a527ede1be87bd715fb3741a81eef5bd422efa':
        raise ValueError('not the acquired NPE-3 PMM')
    descriptors = [(0x24a44, (0xff80, 0xff80, 104, 0x200, 0x8c, 0)),
                   (0x24b70, (0xa00, 0x1000, 629, 0x200, 0x3e8, 0))]
    uploads = []
    for offset, expected in descriptors:
        actual = struct.unpack_from('>6H', flash, offset)
        if actual != expected:
            raise ValueError(f'unexpected own upload descriptor at {offset:x}')
        payload = flash[offset + 12:offset + 12 + actual[2] * 2]
        uploads.append({'descriptor': f'{offset + 0x200000:06x}',
                        'source_offset': f'{offset + 12:x}', 'words': actual[2],
                        'sha1': hashlib.sha1(payload).hexdigest()})
    cache, records, stop = replay(pmm, 0x9c4)
    first = records[0]
    base = pmm[first['source']:first['source'] + first['length']]
    return {'uploads': uploads, 'pmm_records': len(records),
            'pmm_stop': f'{stop:x}', 'pmm_initial_length': first['length'],
            'base_computed': f'{checksum(base):04x}', 'base_stored': base[0x254:0x256].hex(),
            'journal_computed': f'{checksum(cache):04x}',
            'journal_stored': cache[0x254:0x256].hex(),
            'scope': 'static journal grammar comparison; runtime reader still requires validation'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('flash', type=Path)
    parser.add_argument('pmm', type=Path)
    args = parser.parse_args()
    try:
        result = assess(args.flash.read_bytes(), args.pmm.read_bytes())
    except (OSError, ValueError) as error:
        parser.exit(1, f'6210 upload assessment FAIL: {error}\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
