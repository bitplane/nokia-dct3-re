import unittest

from tools.nsb3_static_check import FRAGMENTS, inspect, peripheral_contract


class Nsb3StaticTest(unittest.TestCase):
    def test_own_instructions_literals_and_tables(self):
        image = bytearray(0xd6d00)
        image[5] = 0xf4
        for address, encoded in FRAGMENTS.items():
            data = bytes.fromhex(encoded)
            offset = address - 0x200000
            image[offset:offset + len(data)] = data
        result = peripheral_contract(image)
        self.assertEqual(result['eeprom']['descriptor']['capacity_bytes'], 8192)
        self.assertEqual(result['keypad']['power_column_mask'], '0x10')
        self.assertFalse(result['runtime_or_physical_timing_proved'])
        for address in FRAGMENTS:
            changed = bytearray(image)
            changed[address - 0x200000] ^= 1
            with self.assertRaises(ValueError):
                peripheral_contract(changed)

    def test_rejects_unqualified_image(self):
        with self.assertRaisesRegex(ValueError, 'own normalized'):
            inspect(b'not own MCU')


if __name__ == '__main__':
    unittest.main()
