import struct
import unittest

from tools.c54x_rom4_efr_tables import (
    WIDTHS, check, expected_coverage, expected_crc_selectors, speech_descriptors)


class EfrTablesTest(unittest.TestCase):
    def image(self):
        image = bytearray(0x10000 * 2)
        packing = tuple(expected_coverage().elements())
        struct.pack_into(">272H", image, 0xd8c2 * 2, *packing)
        struct.pack_into(">65H", image, 0xd9d2 * 2, *expected_crc_selectors())
        return image

    def test_standard_dimensions(self):
        self.assertEqual((len(WIDTHS), sum(WIDTHS)), (57, 244))
        self.assertEqual(len(set(speech_descriptors())), 244)
        self.assertEqual(sum(expected_coverage().values()), 272)
        self.assertEqual(expected_coverage()[0x0b0e], 3)

    def test_bounded_claim(self):
        result = check(self.image())
        self.assertFalse(result["packing_order_verified"])
        self.assertFalse(result["native_execution_verified"])

    def test_crc_order_matters(self):
        image = self.image()
        start = 0xd9d2 * 2
        image[start:start + 4] = image[start + 2:start + 4] + image[start:start + 2]
        with self.assertRaisesRegex(ValueError, "CRC selector"):
            check(image)

    def test_bad_coverage(self):
        image = self.image()
        image[0xd8c2 * 2] ^= 1
        with self.assertRaisesRegex(ValueError, "coverage"):
            check(image)

    def test_truncated(self):
        with self.assertRaisesRegex(ValueError, "truncated"):
            check(b"")


if __name__ == "__main__":
    unittest.main()
