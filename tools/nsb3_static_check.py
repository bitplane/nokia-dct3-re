"""Recover own NSB-3 startup, peripheral and dual DSP-catalogue declarations."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsk1_static_check import read_catalogue
from tools.dct3_eeprom_descriptor import eeprom_geometry

MCU_SHA256 = '4ae57f0eed01125efbe0f4e63abf5412b44539069b424f5d6dcf030659a8ebda'
FRAGMENTS = {
    0x2000e4: 'e28f0001e12fff10',
    0x20016c: '0010fbf4',
    0x2b057e: '25802601270446a6200002ab3320791a781c43b2711a',
    0x2b064a: '22012380029b33207819431170197919431171192004781943017019',
    0x2b06bc: '22012380029b33207819439170197919431171192004781943017019',
    0x2b129e: '222821225411222a21044321541121295c0908c9d3fb212d5c08',
    0x2b1374: '00020000',
    0x2b19e4: '21295c890849d3fb212c548846f7',
    0x2b1a4a: '21295c890849d3fb212b548846f7',
    0x2b2a2c: 'b5f04641464ab406',
    0x2b2a60: '2031231f5503202f26e0550620305d0006c00ec006000e0125ff291f',
    0x2b2bc0: '00020000',
    0x2a19c0: '213820ff550820c07220',
    0x2b359e: 'b500f7fffa4428ffd101203ebd0009c12900d009491a7809008a188918404916180838807800bd0021194a1478124351184049135c08bd00',
    0x2b3618: '002d6a140010c482002d69f8',
    0x2d69f8: '3e3e3e3e3e11190102030e170405060f18070809101a0c0a0b',
    0x2d6a14: '3e3e3e3e0d',
}


def peripheral_contract(image):
    for address, encoded in FRAGMENTS.items():
        expected = bytes.fromhex(encoded)
        offset = address - 0x200000
        if image[offset:offset + len(expected)] != expected:
            raise ValueError(f'NSB-3 instruction/table differs at {address:#x}')
    return {'thumb_entry': '0x2000ec', 'stack': '0x10fbf4',
            'eeprom': {'sda_bit': 0, 'scl_bit': 2, 'gpio': '0x20020',
                       'descriptor': eeprom_geometry(image[5])},
            'gensio': {'select': '0x28', 'ccont_select': '0x22', 'write': '0x2a',
                       'receive': '0x2d', 'status': '0x29', 'ready_bit': 2,
                       'lcd_command': '0x2c', 'lcd_data': '0x2b'},
            'keypad': {'scanner': '0x2b2a2c', 'decoder': '0x2b359e',
                       'row': '0x31', 'direction': '0x2f', 'column': '0x30',
                       'normal_table': '0x2d69f8', 'special_table': '0x2d6a14',
                       'selector': '0x10c482', 'power_column_mask': '0x10'},
            'runtime_or_physical_timing_proved': False}


def inspect(image):
    if hashlib.sha256(image).hexdigest() != MCU_SHA256:
        raise ValueError('requires own normalized NSB-3 v6.13 MCU')
    catalogues = []
    for table, identity in ((0x2b67d0, 3), (0x2b6848, 4)):
        result = read_catalogue(image, table)
        last = result['entries'][-1]
        offset = int(last['descriptor'], 16) - 0x200000 + 12
        if (last['header'] != [0xf00, 0, 186, 0x700, 180, 0]
                or image[offset + 24:offset + 30] != bytes.fromhex('76f80803') + struct.pack('>H', identity)):
            raise ValueError('NSB-3 bootstrap identity instruction differs')
        catalogues.append({'table': hex(table), 'bootstrap_identity': identity, **result})
    return {'mcu_sha256': MCU_SHA256, 'peripherals': peripheral_contract(image),
            'catalogues': catalogues,
            'scope': 'Own static declarations only; runtime mask selection and fitted compatibility unproved'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mcu', type=Path)
    args = parser.parse_args()
    try:
        report = inspect(args.mcu.read_bytes())
    except (OSError, ValueError) as exc:
        parser.exit(1, f'NSB-3 static check: {exc}\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
