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
            'keypad': {'scanner': '0x3c0e72', 'row_output': '0x28',
                       'row_direction': '0xa8', 'column_input': '0x2a',
                       'raw_formula': 'row * 5 + column'},
            'simi_init_fragment': '0x385874',
            'eeprom_descriptor': eeprom_geometry(image[5]),
            'physical_key_labels_or_timing_proved': False}


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
