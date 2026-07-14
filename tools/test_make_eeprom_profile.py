#!/usr/bin/env python3
import unittest
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import make_eeprom_profile


class ChecksumTests(unittest.TestCase):
    @staticmethod
    def build() -> bytearray:
        # The copied fallback record reaches approximately 0x0d8000 in flash.
        return make_eeprom_profile.build_profile(bytes(0x0D9000))

    def test_profile_has_valid_firmware_checksums(self):
        image = self.build()
        make_eeprom_profile.validate_checksums(image)

    def test_tune_security_checksum_is_computed_from_content(self):
        image = self.build()
        image[0x0040] ^= 1
        with self.assertRaisesRegex(ValueError, "tune/security checksum mismatch"):
            make_eeprom_profile.validate_checksums(image)

    def test_config_checksum_is_computed_from_content(self):
        image = self.build()
        image[0x0170] ^= 1
        with self.assertRaisesRegex(ValueError, "config checksum mismatch"):
            make_eeprom_profile.validate_checksums(image)

    def test_provisioned_identity_records_match_firmware_derivation(self):
        image = make_eeprom_profile.build_profile(bytes(0x0D9000), "49015420323751")
        self.assertEqual(image[0x000C:0x0014], bytes.fromhex("4901542032375100"))
        self.assertEqual(image[0x0110:0x0113], bytes.fromhex("123450"))
        self.assertEqual(image[0x06C8:0x06D0], bytes.fromhex("32d8fa9700000317"))
        self.assertEqual(make_eeprom_profile.imei_check_digit("49015420323751"), "8")


if __name__ == "__main__":
    unittest.main()
