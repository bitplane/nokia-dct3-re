import hashlib
import io
import struct
import unittest
from unittest.mock import patch
import zipfile

from tools import nse2_package_check as check


def loader_image():
    image = bytearray(0xa1200)
    table = check.TABLE - check.BASE
    struct.pack_into('>II', image, table - 8, 112, 0x10ba14)
    pointers = [0x29bc20] * 27 + [0]
    pointers[25] = 0x2a0f6c
    struct.pack_into('>28I', image, table, *pointers)
    descriptor = pointers[25] - check.BASE
    struct.pack_into('>6H', image, descriptor, 0xf00, 0, 186, 0x700, 180, 0)
    payload = descriptor + 12
    image[payload + 24:payload + 30] = bytes.fromhex('76f808030004')
    for relative, target in ((0x44, 0x8023), (0x7c, 0x8023), (0xf4, 0x802b)):
        struct.pack_into('>HH', image, payload + relative, 0xf074, target)
    return image


def package(members):
    stream = io.BytesIO()
    with zipfile.ZipFile(stream, 'w') as archive:
        for name, data in members:
            archive.writestr(name, data)
    return stream.getvalue()


def peripheral_image():
    image = bytearray(0x190000)
    for address, encoded in check.PERIPHERAL_FRAGMENTS.values():
        value = bytes.fromhex(encoded)
        image[address - check.BASE:address - check.BASE + len(value)] = value
    for address in (0x2967dc, 0x297020, 0x298200, 0x2866b0):
        struct.pack_into('>I', image, address - check.BASE, 0x20000)
    struct.pack_into('>I', image, 0x7df44, 0x10fca0)
    for address, value in ((0x233594, 0x31e), (0x278488, 0xffff), (0x27848c, 0x10ba78),
                           (0x2782f4, 0x10b9f4), (0x28dabc, 0x10b9f4),
                           (0x25c99c, 0x10c5cc), (0x25c800, 0x10ffac),
                           (0x25caf0, 0x10c5c9), (0x264908, 5500), (0x264774, 1500),
                           (0x2975f0, 0x2b3850), (0x22d6e4, 0x10bb0c),
                           (0x264764, 0x10a558), (0x22db34, 0x10a562)):
        struct.pack_into('>I', image, address - check.BASE, value)
    image[0xb3850:0xb385a] = bytes.fromhex('00040506070301020201')
    image[0x9907c:0x99088] = bytes.fromhex('002b3848 0010c6b6 002b382c')
    image[0xb382c:0xb382c + 25] = check.NORMAL_KEYS
    image[0xb3848:0xb3848 + 5] = bytes.fromhex('3e3e3e3e0d')
    return image


class Nse2PackageTest(unittest.TestCase):
    def test_unknown_wrapper_is_rejected_before_extraction(self):
        with self.assertRaisesRegex(ValueError, 'hash-qualified'):
            check.unpack_package(package([('other', b'data')]))

    def test_member_names_sizes_and_hashes_are_all_required(self):
        members = [('mcu', b'abc'), ('ppm', b'1234')]
        expected = {name: (len(data), hashlib.sha256(data).hexdigest())
                    for name, data in members}
        variants = [members, [('other', b'abc'), members[1]],
                    [('mcu', b'ab'), members[1]], [('mcu', b'abd'), members[1]]]
        with patch.dict(check.MEMBERS, expected, clear=True):
            for index, variant in enumerate(variants):
                source = package(variant)
                with self.subTest(index=index), patch.dict(
                        check.ARCHIVES, {check.sha256(source): 'test'}, clear=True):
                    if index == 0:
                        contents, report = check.unpack_package(source)
                        self.assertEqual(contents, dict(members))
                        self.assertEqual(report['container'], 'test')
                    else:
                        with self.assertRaises(ValueError):
                            check.unpack_package(source)

    def test_own_catalogue_keeps_populated_slots_11_and_26(self):
        result = check.loader_contract(loader_image())
        self.assertEqual(result['null_slots'], [27])
        self.assertEqual(len(result['entries']), 27)
        self.assertEqual(result['verifier_identity_store'], 4)
        self.assertEqual(result['resident_call_targets'], ['0x8023', '0x8023', '0x802b'])
        self.assertFalse(result['fitted_mask_or_execution_proved'])

    def test_older_catalogue_shape_is_not_inherited(self):
        for slot in (11, 26):
            image = loader_image()
            struct.pack_into('>I', image, check.TABLE - check.BASE + slot * 4, 0)
            with self.subTest(slot=slot), self.assertRaisesRegex(ValueError, 'null slots'):
                check.loader_contract(image)

    def test_pointer_and_payload_bounds(self):
        for pointer in (check.BASE - 2, check.BASE + 1, check.BASE + 0xa11fe):
            image = loader_image()
            struct.pack_into('>I', image, check.TABLE - check.BASE, pointer)
            with self.subTest(pointer=pointer), self.assertRaisesRegex(ValueError, 'outside MCU'):
                check.loader_contract(image)
        image = loader_image()
        struct.pack_into('>6H', image, 0x9bc20, 0xffff, 0, 2, 0, 0, 0)
        with self.assertRaisesRegex(ValueError, 'extent'):
            check.loader_contract(image)

    def test_verifier_identity_and_calls_are_not_assumed(self):
        payload = 0xa0f6c + 12
        for offset in (24, 29, 0x44, 0x47, 0x7f, 0xf7):
            image = loader_image()
            image[payload + offset] ^= 1
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                check.loader_contract(image)

    def test_truncated_catalogue_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'truncated'):
            check.loader_contract(b'')

    def test_peripheral_report_keeps_static_scope(self):
        result = check.peripheral_contract(peripheral_image())
        self.assertEqual(result['eeprom']['scl_bit'], 2)
        self.assertEqual(result['eeprom']['start'], '0x295814')
        self.assertEqual(result['eeprom']['stop'], '0x295886')
        self.assertEqual(result['eeprom']['block_arguments'], ['address', 'destination', 'length'])
        self.assertEqual(result['eeprom']['final_nack_argument'], 0)
        self.assertEqual(result['gensio']['ccont_tx'], '0x2a')
        self.assertEqual(result['keypad']['power_column'], 4)
        self.assertEqual(result['keypad']['column_irq_mask'], '0x33')
        self.assertEqual(result['keypad']['normal_keys'], list(check.NORMAL_KEYS))
        self.assertFalse(result['electrical_timing_or_runtime_delivery_proved'])
        self.assertEqual(result['analog_readiness']['ccont_channel'], 2)
        self.assertEqual(result['analog_readiness']['accepted_sample_max'], 5500)
        self.assertFalse(result['analog_readiness']['physical_calibration_proved'])

    def test_each_peripheral_instruction_is_qualified(self):
        for name, (address, _) in check.PERIPHERAL_FRAGMENTS.items():
            image = peripheral_image()
            image[address - check.BASE] ^= 1
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, name):
                check.peripheral_contract(image)

    def test_bus_literals_and_table_byte_lanes_are_not_interchangeable(self):
        for offset in (0x967dc, 0x97020, 0x98200, 0x866b0, 0x9907c, 0xb382c, 0xb3848):
            image = peripheral_image()
            image[offset] ^= 1
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                check.peripheral_contract(image)


if __name__ == '__main__':
    unittest.main()
