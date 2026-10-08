"""Normalize the pinned NPM-5 package without executing its installer."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import zipfile

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.extract_dct3_wintesla import decode_records
from tools.dct3_startup_records import decode_startup_records, initialized_bytes

PACKAGE_SHA256 = '4f13d4aab02e970a86e83c15b903aed18590584dc5c383ed126cf632763d8d60'
FLASH_SHA256 = 'a320d808229adf1e471446547d3ec4643d597a1f67f4b0975cc9beada7c3d15a'
UPLOADS = (
    (0x1e9240, (0xff80, 0xff80, 104, 0x200, 0x8c, 0), '440bf49f1eba4cadb12f7f7581c992b0025807d6'),
    (0x1e936c, (0xa00, 0x1000, 629, 0x200, 0x3e8, 0), 'ab334f30a6c564ba0d60a1adb64e95f22620111a'),
    (0x1ea428, (0xf00, 0, 223, 0xf00, 0xdc, 0), '6646da3c5be9c70deda7e0b5b9f257d5d2ace815'),
)


def decode_class_routes(table):
    """Decode the fixed class router, including function-valued destinations."""
    if len(table) != 39 * 8:
        raise ValueError('class router extent mismatch')
    return [{'class': row[0], 'destination': struct.unpack_from('>I', row, 4)[0]}
            for row in (table[offset:offset + 8] for offset in range(0, len(table), 8))]


def thumb_reference_census(image, targets, base=0x200000):
    """Aligned encoded references, not proof against computed or ARM calls."""
    targets = set(targets)
    calls = {target: [] for target in targets}
    pointers = {target: [] for target in targets}
    candidates = 0
    for offset in range(0, len(image) - 3, 2):
        high, low = struct.unpack_from('>HH', image, offset)
        if high & 0xf800 == 0xf000 and low & 0xf800 == 0xf800:
            candidates += 1
            displacement = ((high & 0x7ff) << 12) | ((low & 0x7ff) << 1)
            if displacement & 0x400000:
                displacement -= 0x800000
            target = base + offset + 4 + displacement
            if target in targets:
                calls[target].append(base + offset)
        word = (high << 16) | low
        if word & 1 and (word & ~1) in targets:
            pointers[word & ~1].append(base + offset)
    return {'image_bytes': len(image), 'aligned_pairs_scanned': len(range(0, len(image) - 3, 2)),
            'plausible_bl_pairs': candidates, 'direct_calls': calls,
            'thumb_pointers': pointers,
            'exclusions': ['computed calls', 'ARM instructions', 'unaligned pointers',
                           'code/data classification of candidate pairs']}


def assess_input(flash):
    """Pin the input consumers; table presence is not wiring acceptance."""
    for start, end, digest in (
        (0x3a738c, 0x3a7424, 'ade0727b15abb6de032ea2b7ee6711f3f643f32b5aa8505a0d7561208abf7bf4'),
        (0x39dc14, 0x39dc4c, 'f5f23ae052176f383844aab5ad69926341fe206301c65ffdbda2a37c5e3eb278'),
        (0x335854, 0x335a2e, '595b6b9bed4b25daad0a4d588f34fd0b2fee9cb1109e6fd31652e6e7f961c76a'),
        (0x335632, 0x33565a, '2cff8c5e2680b88198864ca640c58a3b8221bb8faff9dd068656b6e170220eda'),
        (0x3a7424, 0x3a7472, 'd4c4c0a73c2b91e65b0c3cd4200e054aa878b64b0420246587eca59adb29b67d'),
        (0x335c1c, 0x335da4, '38489482d7e1f12921a97751b72c21a878fd990e7d89a2d91bc321717c009ec5'),
        (0x335b12, 0x335b74, 'f09da60f3625c068cecfbc3e288e5a9f6591d57f5cf1f8ebbd58d6377116337f'),
        (0x2f5760, 0x2f59dc, '2bc18cd9fa00b74f9b67a998de83dfed13420174de12d71e9821779f5e3f7056'),
        (0x2f5c8c, 0x2f5d72, 'd59f6cb9da3e39ea01fda78ed3f2c5191fa12f30abdecb9af87844276e2b64ea'),
    ):
        if hashlib.sha256(flash[start - 0x200000:end - 0x200000]).hexdigest() != digest:
            raise ValueError('input consumer code mismatch')
    for address, value in ((0x3a74dc, 0x20000), (0x39dc98, 0x44d164),
                           (0x39dc9c, 0x126ec6), (0x39dca0, 0x44d180)):
        if struct.unpack_from('>I', flash, address - 0x200000)[0] != value:
            raise ValueError('input literal dependency mismatch')
    normal = flash[0x24d164:0x24d164 + 25]
    special = flash[0x24d180:0x24d180 + 5]
    if normal != bytes.fromhex('3e170a3e1a3e180102013e3e0605043e3e0908073e030b190c'):
        raise ValueError('normal key table mismatch')
    if special != bytes.fromhex('3e3c3e3e3e'):
        raise ValueError('special key table mismatch')
    if struct.unpack_from('>I', flash, 0x0ceef0)[0] != 0x419d20:
        raise ValueError('task creation table literal mismatch')
    if flash[0x219e7c:0x219e88] != bytes.fromhex('00335f1f032064280a000000'):
        raise ValueError('serial UI task descriptor mismatch')
    if struct.unpack_from('>I', flash, 0x162d0c)[0] != 0x437ca4:
        raise ValueError('class router literal mismatch')
    routes = decode_class_routes(flash[0x237ca4:0x237ca4 + 39 * 8])
    if [row for row in routes if row['class'] == 0xd2] != [
            {'class': 0xd2, 'destination': 29}]:
        raise ValueError('candidate serial class route mismatch')
    references = thumb_reference_census(flash, (0x2f5760, 0x399fbc))
    if references['direct_calls'] != {0x2f5760: [0x2f5caa],
                                       0x399fbc: [0x2f58ac, 0x2f5994, 0x3841e2]}:
        raise ValueError('serial receive direct-reference census mismatch')
    if struct.unpack_from('>I', flash, 0x219d20 + 8 * 12)[0] != 0x2f5c8d:
        raise ValueError('sequenced receive task descriptor mismatch')
    return {'scope': 'static consumers only; no MU4 matrix wiring validated',
            'sequenced_receive': {'task': 8, 'entry': '2f5c8c', 'receiver': '2f5760',
                                  'internal_selector_offset': 3, 'internal_selector': '8e',
                                  'receiver_call': '2f5caa',
                                  'control_transports': ['1e', '1c'],
                                  'control_handler': '2f52aa',
                                  'reference_census': references,
                                  'physical_byte_source_validated': False},
            'gpio_reader': '3a738c', 'column_register': '2002a',
            'active_low_mask': '02', 'pressed_raw': '81', 'released_raw': 'ff',
            'decoder': '39dc14', 'mode_byte': '126ec6',
            'normal_table': '44d164', 'normal_codes': list(normal),
            'special_table': '44d180', 'special_codes': list(special),
            'mode_zero_pressed_code': '3c', 'released_code': '3e',
            'irq_status_register': '2002b',
            'irq_bit1_handler': '39dae8', 'irq_bit3_handler': '335624',
            'mixed_port_contract': {'initializer': '3a7424',
                                    'direction_register': '2006a', 'direction_value': '24',
                                    'data_register': '2002a', 'initial_data': '3f',
                                    'irq_mask_register': '2006b', 'irq_mask': '75',
                                    'output_writer': '335632', 'output_bit': 2,
                                    'mu4_pin_mapping_validated': False},
            'serial_ui_task': {'table': '419d20', 'index': 29,
                               'descriptor': '419e7c', 'entry': '335f1e',
                               'stack_bytes': 800, 'priority_byte': '64',
                               'creation_count': 30},
            'serial_key_consumer': {'dispatcher': '335854', 'selector_offset': 9,
                                    'selector': '0c', 'state_offset': 10,
                                    'key_offset': 12, 'press_state': 1,
                                    'release_state': 0, 'press_call': '313d84',
                                    'release_call': '313b2c',
                                    'wire_framing_validated': False},
            'mu4_specific_consumer': {'receiver': '335cfa', 'handler': '335c1c',
                                      'source_node': '28', 'transport': '1e',
                                      'incoming_class': 'd2', 'incoming_wrapper': '42',
                                      'opcode_offset': 9, 'key_opcode': '01',
                                      'key_offset': 12, 'state_offset': 10,
                                      'power_up_opcode': '06', 'selftest_reply': '6f',
                                      'selftest_pass_value': 3,
                                      'wire_framing_validated': False},
            'local_class_router': {'consumer': '362958', 'table': '437ca4',
                                   'records': routes, 'decoded_records': 39,
                                   'class_d2_task': 29,
                                   'internal_control_constructor': '362de0',
                                   'external_wire_route_validated': False}}


def assess_bootstrap(flash, records):
    """Pin the selected consumer code and its literal-pool dependencies."""
    for start, end, digest in (
        (0x319032, 0x31913e, '1af8a9f5b694ae9871a2d745a5790d328d12610b078d8d09720076569b6f82e4'),
        (0x31916c, 0x3192e2, '39ad61c02f1aa75c5019a96f001caaae3c27cd333091d89293f956292431f8c7'),
    ):
        if hashlib.sha256(flash[start - 0x200000:end - 0x200000]).hexdigest() != digest:
            raise ValueError('bootstrap consumer code mismatch')
    for address, value in ((0x3192f8, 0x1237f0), (0x3192fc, 0x100f6),
                           (0x319300, 0x20002), (0x319304, 0x100fe),
                           (0x319308, 0x10200), (0x31930c, 0x200040),
                           (0x319248, 0x123760), (0x31931c, 0x123784)):
        if struct.unpack_from('>I', flash, address - 0x200000)[0] != value:
            raise ValueError('bootstrap literal dependency mismatch')
    if flash[0x17be7a:0x17be7e] != bytes.fromhex('f79df8da'):
        raise ValueError('startup verifier call mismatch')
    if initialized_bytes(records, 0x123784, 4) != struct.pack('>I', 0x3e3304):
        raise ValueError('initial loader selection mismatch')
    fields = struct.unpack_from('>6H', flash, 0x1e3304)
    payload = flash[0x1e3310:0x1e3310 + 638 * 2]
    digest = 'dc3c2a37913e07a2962d09f6a17cde676a04dd76'
    if fields != (0xfd00, 0xff80, 638, 0x500, 0x78, 0) or hashlib.sha1(payload).hexdigest() != digest:
        raise ValueError('initial loader descriptor/payload mismatch')
    return {'scope': 'static code and data, not runtime acceptance',
            'verifier_call': '37be7a', 'verifier_result_words': ['12376a', '12376c'],
            'initial_loader_pointer': '123784', 'initial_loader_descriptor': '3e3304',
            'initial_loader_consumer': '319284', 'initial_loader_words': 638,
            'initial_loader_sha1': digest}


def assess_flash(flash):
    if len(flash) != 0x350000 or hashlib.sha256(flash).hexdigest() != FLASH_SHA256:
        raise ValueError('not the pinned NPM-5 v3.53 PPM C image')
    if not flash[0x290000:].startswith(b'PPM\x00V 03.53\n15-04-02\nNPM-5\n'):
        raise ValueError('product/version identifier mismatch')
    uploads = []
    for offset, fields, digest in UPLOADS:
        if struct.unpack_from('>6H', flash, offset) != fields:
            raise ValueError(f'descriptor mismatch at {offset + 0x200000:06x}')
        payload = flash[offset + 12:offset + 12 + fields[2] * 2]
        if hashlib.sha1(payload).hexdigest() != digest:
            raise ValueError('upload extent/hash mismatch')
        uploads.append({'descriptor': f'{offset + 0x200000:06x}',
                        'words': fields[2], 'sha1': digest,
                        'scope': 'static descriptor candidate, not executed ownership proof'})
    records, end = decode_startup_records(flash, 0x200000, 0x3b0430)
    if (len(records), end, sum(len(payload) for _, _, payload in records)) != (2090, 0x3bb8d8, 26487):
        raise ValueError('startup initializer coverage mismatch')
    for address, descriptor in ((0x1237c0, 0x3e9240), (0x1237d4, 0x3e936c),
                                (0x1237f0, 0x3ea428)):
        if initialized_bytes(records, address, 4) != struct.pack('>I', descriptor):
            raise ValueError('initialized upload pointer mismatch')
    return {'product': 'NPM-5', 'version': '3.53', 'ppm': 'C',
            'input_contract': assess_input(flash),
            'bootstrap_contract': assess_bootstrap(flash, records),
            'startup_copy': {'records': len(records), 'payload_bytes': 26487,
                             'terminator_end': f'{end:06x}',
                             'verifier_pointer': '1237f0', 'consumer': '319032'},
            'flash_size': len(flash), 'flash_sha256': FLASH_SHA256,
            'uploads': uploads, 'pmm_present': False,
            'graphical_boot_validated': False, 'native_dsp_complete': False}


def normalize(package):
    if hashlib.sha256(package.read_bytes()).hexdigest() != PACKAGE_SHA256:
        raise ValueError('service-package SHA-256 mismatch')
    with zipfile.ZipFile(package) as archive:
        members = {}
        for name, size in (('npm5nx03.530', 2417759), ('npm5nx03.53c', 787296),
                           ('npm-5.ini', 8273)):
            if archive.getinfo(name).file_size != size:
                raise ValueError('unexpected member extent: ' + name)
            members[name] = archive.read(name)  # ZIP CRC is checked here.
    mcu_base, mcu = decode_records(members['npm5nx03.530'], 'NPM-5 MCU')
    ppm_base, ppm = decode_records(members['npm5nx03.53c'], 'NPM-5 PPM C')
    if (mcu_base, len(mcu), ppm_base, len(ppm)) != (0x200000, 0x24da00, 0x490000, 0xc0000):
        raise ValueError('unexpected MCU/PPM record coverage')
    flash = mcu + b'\xff' * (ppm_base - mcu_base - len(mcu)) + ppm
    report = assess_flash(flash)
    report['package_sha256'] = PACKAGE_SHA256
    report['unprogrammed_gap'] = {'start': '44da00', 'end': '490000', 'fill': 'ff'}
    return members, flash, report


def write_derived(path, data):
    if path.exists() and path.read_bytes() != data:
        raise ValueError('refusing to replace different existing artifact: ' + str(path))
    path.write_bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('package', type=Path)
    parser.add_argument('--output-dir', type=Path)
    args = parser.parse_args()
    try:
        members, flash, report = normalize(args.package)
        if args.output_dir:
            args.output_dir.mkdir(parents=True, exist_ok=True)
            for name, data in members.items():
                write_derived(args.output_dir / name, data)
            write_derived(args.output_dir / '5510f353c.fls', flash)
            # Keep the immutable normalization manifest independent of
            # expanding static-analysis results reported on stdout.
            manifest = {key: value for key, value in report.items()
                        if key not in ('startup_copy', 'bootstrap_contract', 'input_contract')}
            write_derived(args.output_dir / 'normalization.json',
                          (json.dumps(manifest, indent=2) + '\n').encode())
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        parser.exit(1, f'NPM-5 package validation FAIL: {error}\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
