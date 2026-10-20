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
    # ARM LDRB control, TST disable bit, SUBSNE return in each dispatcher.
    for address, word in [(0x2FB57C, 0xE5D0100C), (0x2FB580, 0xE3110008),
                          (0x2FB588, 0x125EF004), (0x2FB6E0, 0xE5DC800C),
                          (0x2FB6E4, 0xE3180002), (0x2FB6E8, 0x125EF004)]:
        if struct.unpack_from('>I', image, address - BASE)[0] != word:
            raise ValueError(f'interrupt-control instruction differs at {address:#x}')
    rows = [struct.unpack_from(">4H", image, TABLE - BASE + index * 8)
            for index in range(10)]
    # Expiry stores state 3, then skips task linkage unless flags bit 0 is set.
    if image[0x2ABCEC - BASE:0x2ABCF8 - BASE].hex() != 'b5302103720179c10849d31f':
        raise ValueError('event-expiry flag guard differs')
    irq3_record = image[0x331030 - BASE:0x331038 - BASE]
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
        "idle_irq3_event": {
            "handler": "0x3075c8", "schedule": "0x2abaf4",
            "cancel": "0x2ab970", "event": 0x79, "delay": 0x4b,
            "initializer": "0x2ac7a8", "count": 0x109,
            "rom_table": "0x330c68", "rom_stride": 8,
            "rom_record": "0x331030",
            "rom_bytes": irq3_record.hex(),
            "runtime_record": "0x101334", "runtime_stride": 12,
            "expiry": "0x2abcec", "task_link_flag": 1,
            "initial_expiry_links_task": bool(irq3_record[0] & 1),
            "physical_source": "unresolved",
            "external_source_label": "MADos include/hw/int.h: IRQ_SLEEP=3",
            "task_destination_proven": False,
        },
        "interrupt_control_consumers": {
            "irq_entry": "0x2fb574", "irq_guard": "0x2fb580",
            "irq_disable_read_bit": 3, "irq_disabled_return": "0x2fb588",
            "fiq_entry": "0x2fb6dc", "fiq_guard": "0x2fb6e4",
            "fiq_disable_read_bit": 1, "fiq_disabled_return": "0x2fb6e8",
            "critical_enter": "0x2fab36", "critical_leave": "0x2fab58",
            "critical_snapshot": "0x10f9f4", "critical_depth": "0x10f9f6",
            "disable_write": 0x0a, "restore_mask": 0x0f,
            "read_masking_justified": False,
        },
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
