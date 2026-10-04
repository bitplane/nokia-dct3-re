import unittest
from hashlib import sha256
from unittest.mock import patch

from tools.noki6250_phonebook_check import check
from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH


class PhonebookCheckTest(unittest.TestCase):
    def data(self):
        data = bytearray([0xff] * CURRENT_NVRAM_LENGTH)
        data[0] = ord("A")
        data[18:22] = bytes((3, 0x81, 0x21, 0xf3))
        return data

    def test_storage_and_pixel_match(self):
        pixels = bytes(96 * 60)
        with patch.dict("tools.noki6250_phonebook_check.ORACLES", save=sha256(pixels).hexdigest()):
            check(self.data(), pixels, (96, 60), "save")

    def test_wrong_pixels_rejected(self):
        with self.assertRaises(ValueError):
            check(self.data(), bytes(96 * 60), (96, 60), "readback")

    def test_corrupt_number_rejected(self):
        data = self.data()
        data[20] = 0
        with self.assertRaises(ValueError):
            check(data, bytes(96 * 60), (96, 60), "save")

    def test_wrong_geometry_rejected(self):
        with self.assertRaises(ValueError):
            check(self.data(), bytes(84 * 48), (84, 48), "save")


if __name__ == "__main__":
    unittest.main()
