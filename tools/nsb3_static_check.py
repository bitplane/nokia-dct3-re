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
from tools.nsm1_v523_static_check import direct_call_candidates

MCU_SHA256 = '4ae57f0eed01125efbe0f4e63abf5412b44539069b424f5d6dcf030659a8ebda'
ACCESSORY_FRAGMENTS = {
    0x2ad13a: 'b5102000f008f881',
    0x2743f0: 'f038fea349e4800849e44288db00e4d4',
    0x273d8c: '7b800880d20349da20107008e387',
    0x273daa: '213901094288da00e3ff7828',
    0x273dc4: '48d67b810889d300e37f7b800880d218',
    0x273dea: '48c478012920d100e3c4212070012002f02cfd46',
    0x2ad1a4: 'b500483c210f70c121007041',
    0x2744ce: '4da67828280fd0062000f02cf9d7200f7028f024',
    0x2740fc: '0010c3cb',
    0x274120: '00020000',
    0x274768: '0010c3cb',
    0x274788: '0010c3d6',
    0x27478c: '00000312',
    0x2747c0: '00020000',
}
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
    0x2946a0: '48cf468088402803d10148e4e00048e46806',
    0x2949e0: '0010ad08',
    0x294a3c: '0010ad9c0010ad2c',
    0x294acc: '88612903d1014962e000496200805809',
    0x294c5c: '0010ad9c0010ad2c',
    0x28dff4: 'b510240048ed78002800d008f7e7fb142800d00448f778002800d000240106200e00bd10',
    0x28e3b0: '0010be1a',
    0x28e3e8: '001080ef',
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
    acquisition = acquisition_contract(image)
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
            'acquisition_contract': acquisition,
            'checksum_contract': checksum_contract(image),
            'accessory_contract': accessory_contract(image),
            'catalogue_selector': {'context': '0x10ad08', 'field': '0x10ad0a',
                                   'equals_3': '0x10ad9c', 'otherwise': '0x10ad2c',
                                   'initial_consumer': '0x2946a0', 'indexed_consumer': '0x294acc'},
            'idle_permission': {'entry': '0x28dff4', 'return': '0x28e016',
                                'first_flag': '0x10be1a', 'second_flag': '0x1080ef',
                                'intermediate_predicate': '0x27562c',
                                'zero_is_not_dsp_wait_proof': True},
            'scope': 'Own static declarations only; runtime mask selection and fitted compatibility unproved'}


def accessory_contract(image):
    for address, encoded in ACCESSORY_FRAGMENTS.items():
        expected = bytes.fromhex(encoded)
        offset = address - 0x200000
        if image[offset:offset + len(expected)] != expected:
            raise ValueError(f'NSB-3 accessory instruction/literal differs at {address:#x}')
    callers = direct_call_candidates(image, 0x2ad13a)
    if callers != [0x2743f0]:
        raise ValueError('NSB-3 selector-zero wrapper direct-call candidates differ')
    return {'raw_selector': 0, 'reader': '0x2ad13a',
            'state': '0x10c3cb', 'sample': '0x10c3d6',
            'decision_threshold': 0x312, 'cleanup_threshold': 0x390,
            'external_status': '0x2000e', 'tested_bit': 1,
            'runtime_comparison_targets': [
                {'adc0': 0, 'external_status': 0, 'state': 0x10, 'title': 'Headset'},
                {'adc0': 1023, 'external_status': 0, 'state': 0x20, 'title': 'Car'},
                {'adc0': 1023, 'external_status': 2, 'state': 0x0f, 'title': None}],
            'reader_direct_call_candidates': [hex(site) for site in callers],
            'indirect_reader_coverage': False,
            'physical_pin_identity_or_voltage_proved': False}


def acquisition_contract(image):
    if hashlib.sha256(image[0x7a3e0:0x7a4b4]).hexdigest() != '12729b59c44f38da04c97bc49e8d65e3bb37b1907f05448e25da2c01a5218de9':
        raise ValueError('NSB-3 complete acquisition helper differs')
    for address, encoded in {
            0x27a69c: '0010fe20000005dc', 0x27a834: '0000157c',
            0x2b1f20: '481d5d40f003f98e', 0x2b1f98: '002d6a1c',
            0x2d6a1c: '00040506070301020201',
            0x2b5244: 'b5301c04f7f6ffa3482021295c090889d3fb21285c0a2322540b21874b1c781b4019012343192308430b06190e09252a23fb4c187824',
            0x2b52d0: '00020000'}.items():
        expected = bytes.fromhex(encoded)
        offset = address - 0x200000
        if image[offset:offset + len(expected)] != expected:
            raise ValueError('NSB-3 acquisition channel/constant roots differ')
    return {'helper': '0x27a3e0', 'accumulator_return': '0x27a4c2',
            'source': 7, 'adc_channel': 2, 'numerator': 1500, 'divisor': 229,
            'inclusive_sample_window': [1800, 5500],
            'gain_ram': '0x10fe54', 'offset_ram': '0x10fe58',
            'remaining_ram': '0x10a0e2', 'physical_units_proved': False}


def checksum_contract(image):
    for start, end, digest in (
            (0x23ce68, 0x23cede, 'ac9c83cc3e3d1a6a6bee0586987783fedfa301f8a8a23958bf580ec0fafa6102'),
            (0x270aa4, 0x270ade, '12a41b15bad88563dc155336e3a65ef9eb1f59ac7a26689438d926f0af5f4017'),
            (0x2a5b14, 0x2a5b3a, '9f0e8d5aaa2f85e9c4b80238ab30f41023dff68b0c4d961e80b8e3da394c3a12')):
        if hashlib.sha256(image[start - 0x200000:end - 0x200000]).hexdigest() != digest:
            raise ValueError('NSB-3 decoded EEPROM checksum helper differs')
    return {'security': {'reader': '0x270aa4', 'sum_bytes': ['0x0', '0x11c'],
                         'stored': ['0x11c', '0x120'], 'arithmetic': 'sum16 zero extended to BE32'},
            'calibration': {'reader': '0x23ce68', 'sum_bytes': ['0x120', '0x1fe'],
                            'excluded_bytes': ['0x154', '0x156'], 'stored': ['0x1fe', '0x200']},
            'selftest_flag': '0x10fde9', 'aggregate_errors': '0x10fca0',
            'scope': 'Integrity only, not calibrated or authentic handset identity'}


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
