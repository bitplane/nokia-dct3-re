import unittest

from tools.dct3_eeprom_descriptor import eeprom_geometry


class DescriptorTest(unittest.TestCase):
    def test_own_product_descriptors_keep_distinct_geometry(self):
        for value,expected in ((0x92,(1,2,2048)),(0xf4,(2,32,8192))):
            result = eeprom_geometry(value)
            self.assertEqual(expected,tuple(result[key] for key in
                ('address_bytes','page_bytes','capacity_bytes')))
            self.assertFalse(result['fitted_part_proved'])

    def test_invalid_byte_and_shift_rejected(self):
        for value in (-1,256,'92',0):
            with self.subTest(value=value),self.assertRaises(ValueError):
                eeprom_geometry(value)


if __name__ == '__main__':
    unittest.main()
