import unittest

from tools.nsb3_static_check import (FRAGMENTS, ACCESSORY_FRAGMENTS, inspect,
                                    peripheral_contract, accessory_contract)


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

    def test_accessory_branch_bytes_literals_and_direct_reader_coverage(self):
        image = bytearray(0xd6d00)
        for address, encoded in ACCESSORY_FRAGMENTS.items():
            value = bytes.fromhex(encoded)
            image[address - 0x200000:address - 0x200000 + len(value)] = value
        result = accessory_contract(image)
        self.assertEqual(result['reader_direct_call_candidates'], ['0x2743f0'])
        self.assertEqual(result['decision_threshold'], 786)
        self.assertEqual(result['tested_bit'], 1)
        self.assertFalse(result['physical_pin_identity_or_voltage_proved'])
        for address in ACCESSORY_FRAGMENTS:
            changed = bytearray(image)
            changed[address - 0x200000] ^= 1
            with self.subTest(address=address), self.assertRaises(ValueError):
                accessory_contract(changed)


if __name__ == '__main__':
    unittest.main()
