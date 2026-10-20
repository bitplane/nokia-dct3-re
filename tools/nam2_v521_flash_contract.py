#!/usr/bin/env python3
"""Read the acquired NAM-2 flash algorithm selector, not a geometry spec."""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib

if __package__ in (None, ''):
    from extract_dct3_wintesla import decode_records
else:
    from .extract_dct3_wintesla import decode_records


SHA1 = "b7e30deb4393a76d509f843d25ecf20881bc25bb"
TABLE = 0x33D748
BASE = 0x200000
INSTALLER_SHA256 = '8d53e0ccced4c510707ee752f2b66129f033fc8762082e7f4b29a80653d2d99a'


def check_package(installer, image):
    """Qualify the complete executable prefix against separate installer members."""
    inspect(image)
    if hashlib.sha256(installer).hexdigest() != INSTALLER_SHA256:
        raise ValueError('requires the acquired NAM-2 v5.21 installer')
    report = []
    for name, offset, address, size in [
            ('mcu', 0x49AA6D, 0x200000, 0x160000),
            ('matching_ppm', 0x2080B8, 0x360000, 0x90000)]:
        decoder = zlib.decompressobj(31)
        stream = decoder.decompress(installer[offset:], 2 * 1024 * 1024)
        if not decoder.eof:
            raise ValueError(f'{name}: gzip member incomplete or exceeds bound')
        first, payload = decode_records(stream)
        if first != address or len(payload) != size:
            raise ValueError(f'{name}: record span differs')
        if payload != image[address - BASE:address - BASE + size]:
            raise ValueError(f'{name}: executable bytes differ')
        report.append({'member': name, 'offset': hex(offset),
                       'address': hex(address), 'bytes': size,
                       'sha256': hashlib.sha256(payload).hexdigest()})
    return {'installer_sha256': INSTALLER_SHA256, 'equal_prefix_bytes': 0x1F0000,
            'members': report, 'pmm_provenance_claim': False}


def select_family(rows, manufacturer, device, third):
    """Mirror 0x305b0c: scan all ten rows and retain the last match."""
    selected = 0
    for family, *identifiers in rows:
        if all(expected == 0xFFFF or expected == actual
               for expected, actual in zip(identifiers, (manufacturer, device, third))):
            selected = family
    return selected


def inspect(image):
    if hashlib.sha1(image).hexdigest() != SHA1:
        raise ValueError("requires the unchanged acquired NAM-2 v5.21 image")
    rows = [struct.unpack_from(">4H", image, TABLE - BASE + index * 8)
            for index in range(10)]
    return {
        "sha1": SHA1,
        "selector": "0x305b0c",
        "table": f"0x{TABLE:x}",
        "rows": rows,
        "observed_id": [0xB0, 0xD0, 0],
        "selected_family": select_family(rows, 0xB0, 0xD0, 0),
        "intel_c3_top_candidate": {
            "manufacturer": 0x89, "device": 0x88C2,
            "selected_family": select_family(rows, 0x89, 0x88C2, 0),
            "source": "0x33e1a8", "length": 0x174,
            "program": "0x1351f1", "erase": "0x135249",
            "geometry_bytes": {"main": 0x10000, "parameter": 0x2000,
                               "parameter_count": 8, "size": 0x200000},
            "requires": "C3 block unlock/lock and lock-status semantics",
            "fitted_part_proven": False,
        },
        "family_1": {
            "source": "0x33e064", "length": 0x144,
            "destination": "0x1351f0",
            "program": "0x1351f1", "erase": "0x135239",
            "dispatch_slots": "0x10fac8",
        },
        "erase_geometry": "not established by algorithm selection",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument('--installer', type=Path)
    args = parser.parse_args()
    try:
        result = inspect(args.image.read_bytes())
        if args.installer:
            result['package_comparison'] = check_package(
                args.installer.read_bytes(), args.image.read_bytes())
    except (OSError, ValueError, zlib.error) as error:
        parser.error(str(error))
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
