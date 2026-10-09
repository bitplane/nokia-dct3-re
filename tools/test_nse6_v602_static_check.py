import unittest
from pathlib import Path

from tools import nse6_v602_static_check as check


class Nse6StaticTests(unittest.TestCase):
    def test_eeprom_descriptor_own_fields(self):
        self.assertEqual(check.eeprom_descriptor(0xF6), {
            "capacity_bytes": 32768, "page_bytes": 32, "address_bytes": 2})
        self.assertEqual(check.eeprom_descriptor(0xB6)["address_bytes"], 1)
        with self.assertRaises(ValueError):
            check.eeprom_descriptor(0)

    def test_unknown_package_rejected(self):
        with self.assertRaisesRegex(ValueError, "package"):
            check.normalize(b"not firmware")

    def test_unknown_image_rejected(self):
        with self.assertRaisesRegex(ValueError, "image"):
            check.check(bytes(0x200000))

    def test_big_endian_word_and_bounds(self):
        self.assertEqual(check.read32(bytes.fromhex("12345678"), check.BASE),
                         0x12345678)
        for address in (check.BASE - 1, check.BASE + 1):
            with self.assertRaises(ValueError):
                check.read32(bytes(4), address)

    def test_acquired_package_when_available(self):
        root = Path(__file__).resolve().parents[1]
        path = root / "roms/archive-dct3-packages/nse6_602.exe"
        if not path.exists():
            self.skipTest("acquired NSE-6 package absent")
        image = check.normalize(path.read_bytes())
        result = check.check(image)
        self.assertEqual(result["reset_literals"]["0x200068"], "0x125f30")
        self.assertFalse(result["runtime_acceptance"])
        self.assertEqual(result["eeprom_tx"]["scl_bit"], 2)
        self.assertFalse(result["dsp_verifier"]["resident_mask_proven"])
        self.assertEqual(result["dsp_verifier"]["full_blocks"], 127)
        self.assertEqual(result["eeprom_descriptor"]["page_bytes"], 32)
        mutated = bytearray(image)
        mutated[0x168] ^= 1
        with self.assertRaises(ValueError):
            check.check(mutated)
