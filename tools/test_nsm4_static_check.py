import struct
import unittest

from tools.nsm4_static_check import (TABLE, inspect, read_catalogue,
                                    peripheral_contract, PERIPHERAL_FRAGMENTS)


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


if __name__ == '__main__':
    unittest.main()
