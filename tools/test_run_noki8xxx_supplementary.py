from types import SimpleNamespace
import unittest
from unittest.mock import patch

from tools.run_noki8xxx_supplementary import KEY_TABLE, PROFILES, verify_inputs


class OwnInputTest(unittest.TestCase):
    def test_rejects_wrong_mcu(self):
        for product in PROFILES:
            with self.subTest(product=product), self.assertRaisesRegex(ValueError, 'MCU'):
                verify_inputs(product, b'wrong MCU', b'wrong PMM')

    def test_own_pmm_and_table_are_required(self):
        for product, profile in PROFILES.items():
            image = bytearray(0x1d0000)
            offset = profile[5] - 0x200000
            image[offset:offset + len(KEY_TABLE)] = KEY_TABLE
            for bad_pmm, bad_table in ((False, False), (True, False), (False, True)):
                def digest(value):
                    expected = profile[2] if value is image else profile[4]
                    if value is not image and bad_pmm:
                        expected = 'wrong PMM hash'
                    return SimpleNamespace(hexdigest=lambda: expected)
                if bad_table:
                    image[offset] ^= 1
                with self.subTest(product=product, bad_pmm=bad_pmm, bad_table=bad_table), \
                        patch('tools.run_noki8xxx_supplementary.hashlib.sha1', side_effect=digest):
                    if bad_pmm or bad_table:
                        with self.assertRaisesRegex(ValueError, 'PMM|keypad'):
                            verify_inputs(product, image, b'pmm')
                    else:
                        verify_inputs(product, image, b'pmm')


if __name__ == '__main__':
    unittest.main()
