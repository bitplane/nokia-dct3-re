import struct
import unittest

from tools.nsm4_static_check import (TABLE, inspect, read_catalogue,
                                    peripheral_contract, PERIPHERAL_FRAGMENTS,
                                    keymap_contract, catalogue_consumer,
                                    storage_display_contract, STORAGE_DISPLAY_FRAGMENTS,
                                    flash_program_contract)


class Nsm4StaticTest(unittest.TestCase):
    def fixture(self):
        image = bytearray(TABLE - 0x200000 + 124)
        struct.pack_into('>II', image, TABLE - 0x200000, 0x74, 0x13a9a8)
        struct.pack_into('>29I', image, TABLE - 0x200000 + 8,
                         *([0x200000] * 28), 0)
        struct.pack_into('>6H', image, 0, 0xf00, 0, 2, 0xf00, 0, 0)
        image[12:16] = b'ABCD'
        return image

    def test_all_descriptors_recovered(self):
        result = read_catalogue(self.fixture())
        self.assertEqual(len(result), 28)
        self.assertEqual(result[-1]['selector'], 27)
        self.assertEqual(result[0]['declared_destination'], [0xf00, 0xf02])

    def test_bounds_and_terminator_rejected(self):
        for offset, value in [(TABLE - 0x200000 + 120, 1),
                              (TABLE - 0x200000 + 8, 0x1fffff),
                              (TABLE - 0x200000, 0x70)]:
            image = self.fixture()
            struct.pack_into('>I', image, offset, value)
            with self.assertRaises(ValueError):
                read_catalogue(image)
        with self.assertRaises(ValueError):
            read_catalogue(b'')
        image = self.fixture()
        struct.pack_into('>H', image, 0, 0xffff)
        with self.assertRaises(ValueError):
            read_catalogue(image)

    def test_whole_image_pin(self):
        with self.assertRaisesRegex(ValueError, 'own normalized'):
            inspect(self.fixture())

    def test_peripheral_instructions_and_literals_are_load_bearing(self):
        image = bytearray(0x290000)
        image[5] = 0xfd
        for address, encoded in PERIPHERAL_FRAGMENTS.items():
            fragment = bytes.fromhex(encoded)
            offset = address - 0x200000
            image[offset:offset + len(fragment)] = fragment
        result = peripheral_contract(image)
        self.assertEqual(result['gensio']['ccont_ready_bit'], 2)
        self.assertEqual(result['keypad']['row_direction'], '0xa8')
        self.assertEqual(result['eeprom_descriptor']['capacity_bytes'], 16384)
        self.assertFalse(result['physical_key_labels_or_timing_proved'])
        for address in PERIPHERAL_FRAGMENTS:
            changed = bytearray(image)
            changed[address - 0x200000] ^= 1
            with self.assertRaises(ValueError):
                peripheral_contract(changed)

    def test_keymap_roots_tables_and_instructions(self):
        image = bytearray(0x290000)
        image[0x1c5fa4:0x1c5fdc] = bytes.fromhex('b500f7faff5f28ffd101203ebd0009c12900d009493d7809008a18891840493c180838807800bd0021194a3878124351184049355c08bd00')
        struct.pack_into('>III', image, 0x1c60ac, 0x45dfd4, 0x13de7b, 0x45dff0)
        image[0x25dfd4:0x25dfed] = bytes.fromhex('3e3e3e3e3e11190102030e170405060f18070809101a0c0a0b')
        image[0x25dff0:0x25dff5] = bytes.fromhex('3e3e3e3e0d')
        result = keymap_contract(image)
        self.assertEqual(result['layout_zero_keys'][8], 2)
        self.assertFalse(result['runtime_delivery_proved'])
        for offset in (0x1c5fa4, 0x1c60ac, 0x25dfd4, 0x25dff4):
            changed = bytearray(image)
            changed[offset] ^= 1
            with self.assertRaises(ValueError):
                keymap_contract(changed)

    def test_catalogue_consumer_rejects_pointer_or_opcode_drift(self):
        image = bytearray(0x290000)
        image[0x13e95e:0x13e990] = bytes.fromhex('488e00b15842488d61202000004169235a8e52ce1c4006000e002805dbf6889080e088d000401828616089108120320c6122')
        image[0x13eb98:0x13eba0] = bytes.fromhex('0013a9a8000100f6')
        self.assertEqual(catalogue_consumer(image)['payload_offset_bytes'], 12)
        for offset in (0x13e960, 0x13e97c, 0x13e98c, 0x13eb98):
            changed = bytearray(image)
            changed[offset] ^= 1
            with self.assertRaises(ValueError):
                catalogue_consumer(changed)

    def test_storage_and_display_boundaries(self):
        image = bytearray(0x290000)
        for address, encoded in STORAGE_DISPLAY_FRAGMENTS.items():
            data = bytes.fromhex(encoded)
            offset = address - 0x200000
            image[offset:offset + len(data)] = data
        result = storage_display_contract(image)
        self.assertEqual(result['display']['banks'], 6)
        self.assertEqual(result['display']['bytes_per_bank'], 84)
        self.assertFalse(result['storage']['physical_backend_established'])
        for address in STORAGE_DISPLAY_FRAGMENTS:
            changed = bytearray(image)
            changed[address - 0x200000] ^= 1
            with self.assertRaises(ValueError):
                storage_display_contract(changed)

    def test_flash_program_requires_actual_pinned_payload(self):
        with self.assertRaisesRegex(ValueError, 'program routine differs'):
            flash_program_contract(bytearray(0x290000))


if __name__ == '__main__':
    unittest.main()
