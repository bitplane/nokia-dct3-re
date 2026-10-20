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


def sleep_accounting_contract(image):
    """Follow the actual R2 literal reload, not the routine's earlier MMIO base."""
    load=0x2fada2
    opcode=struct.unpack_from('>H',image,load-BASE)[0]
    if opcode&0xff00!=0x4a00:
        raise ValueError('sleep-accounting R2 literal load differs')
    literal=((load+4)&~3)+(opcode&0xff)*4
    base=struct.unpack_from('>I',image,literal-BASE)[0]
    increment=struct.unpack_from('>H',image,0x2fada6-BASE)[0]
    divisor=struct.unpack_from('>H',image,0x2fadba-BASE)[0]
    if increment&0xff00!=0x3000 or divisor&0xff00!=0x2300 or not divisor&0xff:
        raise ValueError('sleep-accounting increment/divisor instructions differ')
    for address in (0x2fadbe,0x2fadca):
        hi,lo=struct.unpack_from('>2H',image,address-BASE)
        displacement=((hi&0x7ff)<<12)|((lo&0x7ff)<<1)
        if displacement&0x400000:displacement-=0x800000
        if hi&0xf800!=0xf000 or lo&0xf800!=0xf800 or address+4+displacement!=0x307098:
            raise ValueError('sleep-accounting divide call target differs')
    # Both signed-divide exits restore R2 from the entry save set.
    for address,word in ((0x307098,0xb43c),(0x307140,0xbc3c),(0x307146,0xbc3c)):
        if struct.unpack_from('>H',image,address-BASE)[0]!=word:
            raise ValueError('divide helper R2 preservation differs')
    stores=[]
    for address in (0x2fadac,0x2fadce):
        word=struct.unpack_from('>H',image,address-BASE)[0]
        if word&0xf800!=0x8000 or (word>>3)&7!=2 or word&7:
            raise ValueError('sleep-accounting STRH source/base differs')
        target=base+((word>>6)&31)*2
        stores.append({'address':hex(address),'opcode':f'{word:04x}',
                       'target':hex(target),'is_mad2_mmio':0x20000<=target<0x20100})
    return {'helper':'0x2fad22','base_reload':hex(load),'base_literal':hex(literal),
            'base':hex(base),'increment':increment&0xff,'divisor':divisor&0xff,
            'divide_preserves_r2':True,'stores':stores}


def idle_control_contract(image):
    """Qualify the two idle exits and the complete clock helper separately."""
    blocks = (
        (0x2f8658, '2005732078282800d0cb78282800d0c8e7fb'),
        (0x2f8688, '2800d0b3f002fb492001e7b0'),
        (0x304c2e, 'b5102800d1072002f002fc3d490d7b4a20fe4010e008'
                     '24021c20f002fc3449087b484320104000407348bd10'),
    )
    for address, expected in blocks:
        payload = bytes.fromhex(expected)
        if image[address-BASE:address-BASE+len(payload)] != payload:
            raise ValueError(f'idle control-flow block differs at {address:#x}')
    # Both arms converge on STRB [R1+0xd]; resolve both PC-relative bases.
    bases = []
    for address in (0x304c3a, 0x304c4c):
        word = struct.unpack_from('>H', image, address-BASE)[0]
        literal = ((address+4)&~3)+(word&0xff)*4
        bases.append(struct.unpack_from('>I', image, literal-BASE)[0])
    if bases != [0x20000, 0x20000]:
        raise ValueError('clock-helper MMIO base differs')
    return {'explicit_enable': '0x2f865a', 'enable_value': 5,
            'guard_return': '0x2f8688',
            'zero_guard_target': '0x2f85f4',
            'nonzero_guard_target': '0x2f85f6',
            'clock_helper': '0x304c2e', 'clock_store': '0x304c56',
            'clock_store_target': '0x2000d',
            'both_helper_arms_clear_bit0': True,
            'nonzero_argument_sets_bit1': True,
            'helper_restores_global_delivery': False,
            'all_idle_exit_traverses_explicit_enable': False}


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
        "software_sleep_accounting_writers": sleep_accounting_contract(image),
        "idle_control_flow": idle_control_contract(image),
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
