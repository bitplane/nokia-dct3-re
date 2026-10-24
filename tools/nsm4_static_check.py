"""Recover NSM-4 v5.18 startup and declared DSP uploads, not fitted silicon."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.dct3_eeprom_descriptor import eeprom_geometry

MCU_SHA256 = 'bf80e6961d37e071c73b0ec5eba448889b4134fa080dfd7f4a6dd04f3a69aac2'
TABLE = 0x3e8e60

# Own instructions and their literal pools; matching an opcode alone does not
# establish which register window the operation addresses.
PERIPHERAL_FRAGMENTS = {
    0x3c0e68: 'b5f04641464a4653465c',
    0x3c0e9a: '4ead201e7030',
    0x3c0eb2: '78b006c00ec006000e0224ff2a1f',
    0x3c0f2a: '40884210d10800a81828180806000e04',
    0x3c1150: '00020028000200a8',
    0x3ca4f4: '4d6b212d20225548',
    0x3ca51a: '206d5d4008c0d3fb206c5d40',
    0x3ca6a4: '00020000',
    0x3cda62: '497a20217008',
    0x3cda6a: '4a79216d5c890849d3fb216e548846f7',
    0x3cdad6: '4a5e216d5c890849d3fb212e548846f7',
    0x3cdc4c: '0002002d00020000',
    0x385874: '213820ff550820c07220',
}


def peripheral_contract(image):
    for address, encoded in PERIPHERAL_FRAGMENTS.items():
        expected = bytes.fromhex(encoded)
        offset = address - 0x200000
        if image[offset:offset + len(expected)] != expected:
            raise ValueError(f'NSM-4 peripheral instruction/literal differs at {address:#x}')
    return {'gensio': {'select': '0x2d', 'ccont_select': '0x22',
                       'ccont_write': '0x2c', 'ccont_read': '0x6c',
                       'status': '0x6d', 'ccont_ready_bit': 2,
                       'lcd_select': '0x21', 'lcd_command': '0x6e',
                       'lcd_data': '0x2e', 'lcd_ready_bit': 0},
            'keypad': {'scanner': '0x3c0e68', 'row_output': '0x28',
                       'row_direction': '0xa8', 'column_input': '0x2a',
                       'raw_formula': 'row * 5 + column'},
            'simi_init_fragment': '0x385874',
            'eeprom_descriptor': eeprom_geometry(image[5]),
            'physical_key_labels_or_timing_proved': False}


def keymap_contract(image):
    decoder = bytes.fromhex('b500f7faff5f28ffd101203ebd0009c12900d009493d7809008a18891840493c180838807800bd0021194a3878124351184049355c08bd00')
    if image[0x1c5fa4:0x1c5fdc] != decoder:
        raise ValueError('NSM-4 key translation instructions differ')
    normal, selector, special = struct.unpack_from('>III', image, 0x1c60ac)
    if (normal, selector, special) != (0x45dfd4, 0x13de7b, 0x45dff0):
        raise ValueError('NSM-4 key translation roots differ')
    keys = bytes.fromhex('3e3e3e3e3e11190102030e170405060f18070809101a0c0a0b')
    if (image[normal - 0x200000:normal - 0x200000 + 25] != keys
            or image[special - 0x200000:special - 0x200000 + 5] != bytes.fromhex('3e3e3e3e0d')):
        raise ValueError('NSM-4 key translation tables differ')
    return {'decoder': '0x3c5fa4', 'normal_table': hex(normal),
            'special_table': hex(special), 'layout_selector': hex(selector),
            'layout_zero_keys': list(keys), 'power_raw': '0x84',
            'power_logical': '0x0d', 'runtime_delivery_proved': False}


def catalogue_consumer(image):
    expected = bytes.fromhex('488e00b15842488d61202000004169235a8e52ce1c4006000e002805dbf6889080e088d000401828616089108120320c6122')
    if (image[0x13e95e:0x13e990] != expected
            or image[0x13eb98:0x13eba0] != bytes.fromhex('0013a9a8000100f6')):
        raise ValueError('NSM-4 indexed catalogue consumer differs')
    return {'lookup': '0x33e95e', 'table_ram': '0x13a9a8',
            'selector_stride_bytes': 4, 'copied_header_halfwords': 5,
            'payload_offset_bytes': 12, 'transport_execution_proved': False}


STORAGE_DISPLAY_FRAGMENTS = {
    0x3383b8: '2000210122c9a3b1f051f8b520002102f051f855682c',
    0x338684: '454550524f4d00c0',
    0x389250: '48d068013c011c2046fe4708',
    0x38927e: '48eb6802466888011c2046fe4710',
    0x389594: '0013c45c',
    0x38962c: '0013c458',
    0x3dbb52: '1e412900d00d39022900d10a490e4a13604a4a13600a4a13610a4a13608a4a13e0094a094908604a4a09600a4a09610a4a09608a4a0960cab002bd00',
    0x3dbb98: '0013c458003e04990015a6b1003e049d0015a739003e04bd003e04fd0015a6b1003e05010015a739003e0521',
    0x3e0498: '880046f7',
    0x3e04fc: '880046f7',
    0x3cdb50: '206d5d000840d3fb216e202455082300258022002040431006000e01206d5d000840d3fb206e5501206d5d000840d3fb206e55052054216d5d090849d3fb212e550b38012800d1f61c5006000e022a06dbe0206d5d000840d3fb216e20205508',
    0x3cdc50: '00020000',
}


def storage_display_contract(image):
    for address, encoded in STORAGE_DISPLAY_FRAGMENTS.items():
        expected = bytes.fromhex(encoded)
        offset = address - 0x200000
        if image[offset:offset + len(expected)] != expected:
            raise ValueError(f'NSM-4 storage/display instructions differ at {address:#x}')
    return {'storage': {'named_region': 'EEPROM', 'registration_call': '0x3383c0',
                        'registration_argument_r2': '0xc9',
                        'word_read_callback_ram': '0x13c45c',
                        'word_program_callback_ram': '0x13c458',
                        'callback_initializer': '0x3dbb52',
                        'read_entries': ['0x3e0498', '0x3e04fc'],
                        'read_operation': 'memory-mapped halfword',
                        'program_entry_ram': '0x15a6b0',
                        'physical_backend_established': False},
            'display': {'clear_loop': '0x3cdb50', 'banks': 6, 'bytes_per_bank': 84,
                        'command_space_width': 84, 'command_space_height': 48,
                        'clear_entry_command': '0x24', 'clear_exit_command': '0x20',
                        'physical_panel_identification_proved': False}}


def flash_program_contract(image):
    variants = []
    for start, end, digest, extra in (
            (0x45e8fc, 0x45e974, 'a0ac023aa4dcc2e3d7cb5d07416faf152b1c9d50332b5553aa39fa8bcc41a29c', False),
            (0x45e974, 0x45e9fc, 'dfb2c0dd075af5bfe199b841c116afec5912f2d73cb1e38d219de59ab625412c', True)):
        body = image[start - 0x200000:end - 0x200000]
        if hashlib.sha256(body).hexdigest() != digest:
            raise ValueError('NSM-4 RAM flash program routine differs')
        if struct.unpack('>4I', body[-16:]) != (0x5fff00, 0x580000, 0x600000, 0x20073):
            raise ValueError('NSM-4 flash program literal bounds differ')
        variants.append({'source': hex(start), 'end_exclusive': hex(end),
                         'bytes': len(body), 'sha256': digest,
                         'additional_unlock_relock_commands': extra})
    for address, expected in (
            (0x3e04b0, (0x45e8fc, 0x45e974, 0x15a6b0)),
            (0x3e0514, (0x45e974, 0x45e9fc, 0x15a6b0))):
        if struct.unpack_from('>3I', image, address - 0x200000) != expected:
            raise ValueError('NSM-4 flash program copy roots differ')
    return {'execution_address': '0x15a6b0', 'variants': variants,
            'program_window': ['0x580000', '0x600000'],
            'command_address': '0x5fff00', 'command_family': 'Intel-style word program',
            'fitted_part_or_identification_table_proved': False}


def dsp_upload_contract(image, entries):
    if hashlib.sha256(image[0x13e234:0x13e332]).hexdigest() != 'add83f26bca92d2414ed3d01300968ac4a7a56b34dcaea6cb6b72b2808d531b6':
        raise ValueError('NSM-4 MCU DSP upload sequence differs')
    if image[0x13e50c:0x13e530] != bytes.fromhex('0013aa14000100f60000ffff0002000200010200000100fe000106000020004000000dff'):
        raise ValueError('NSM-4 MCU DSP upload roots differ')
    if (entries[15]['descriptor'] != '0x4007f0'
            or entries[15]['header'] != [0xff80, 0xff80, 104, 0x200, 140, 0]
            or entries[15]['sha256'] != 'e972f50bfce7866c64676357f5baeed679445fafd052029ab900504c5965b52a'):
        raise ValueError('NSM-4 own program fragment differs')
    return {'fragment_source': '0x4007fc', 'fragment_words': 104,
            'verifier_words': 246, 'release_fields': [0x100, 0x300, 0x1c, 0, 1, 1, 0x200],
            'full_blocks': 0xdff, 'source_start': '0x200040', 'source_stride': '0x400',
            'alternating_buffers': ['0x10200', '0x10600'],
            'ownership_cells': ['0x100fe', '0x10100'],
            'first_unacknowledged_wait': '0x33e2e0'}


def read_catalogue(image):
    offset = TABLE - 0x200000
    if len(image) < offset + 124:
        raise ValueError('NSM-4 catalogue outside image')
    size, destination = struct.unpack_from('>II', image, offset)
    pointers = struct.unpack_from('>29I', image, offset + 8)
    if (size, destination) != (0x74, 0x13a9a8) or pointers[-1] != 0:
        raise ValueError('NSM-4 catalogue initializer differs')
    entries = []
    for selector, pointer in enumerate(pointers[:-1]):
        start = pointer - 0x200000
        if start < 0 or start + 12 > len(image):
            raise ValueError('NSM-4 descriptor outside image')
        header = struct.unpack_from('>6H', image, start)
        end = start + 12 + header[2] * 2
        if end > len(image) or header[0] + header[2] > 0x10000:
            raise ValueError('NSM-4 declared payload exceeds bounds')
        payload = image[start + 12:end]
        entries.append({'selector': selector, 'descriptor': hex(pointer),
                        'header': list(header), 'sha256': hashlib.sha256(payload).hexdigest(),
                        'declared_destination': [header[0], header[0] + header[2]]})
    return entries


def inspect(image):
    if hashlib.sha256(image).hexdigest() != MCU_SHA256:
        raise ValueError('requires own normalized NSM-4 v5.18 MCU')
    entries = read_catalogue(image)
    bootstrap = entries[27]
    if (bootstrap['descriptor'] != '0x4019d8'
            or bootstrap['header'] != [0xf00, 0, 246, 0xf00, 220, 0]
            or bootstrap['sha256'] != '320c8b9c163e13491e0b867b24c2966e10bc51ad96beb2329e6d43eb32d8a9e2'):
        raise ValueError('NSM-4 bootstrap differs')
    return {'mcu_sha256': MCU_SHA256, 'catalogue': hex(TABLE),
            'catalogue_destination': '0x13a9a8', 'entries': entries,
            'reset_arm_entry': '0x200040', 'thumb_entry': '0x200148',
            'startup_calls': ['0x2ea064', '0x23f900', '0x3ab530'],
            'peripheral_contract': peripheral_contract(image),
            'keymap_contract': keymap_contract(image),
            'catalogue_consumer': catalogue_consumer(image),
            'storage_display_contract': storage_display_contract(image),
            'flash_program_contract': flash_program_contract(image),
            'dsp_upload_contract': dsp_upload_contract(image, entries),
            'bootstrap': {'identity_data_0803': 6, 'resident_call': '0x861f',
                          'fitted_mask_compatibility_proved': False},
            'scope': 'Static MCU declarations only; no upload execution, peripheral wiring, PMM or resident mask established'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mcu', type=Path)
    args = parser.parse_args()
    try:
        report = inspect(args.mcu.read_bytes())
    except (OSError, ValueError) as exc:
        parser.exit(1, f'NSM-4 static check: {exc}\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
