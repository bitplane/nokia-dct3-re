#!/usr/bin/env python3
import unittest
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import make_eeprom_profile


class ChecksumTests(unittest.TestCase):
    @staticmethod
    def firmware_fixture() -> bytes:
        # The copied fallback record reaches approximately 0x0d8000 in flash.
        flash = bytearray(0x0D9000)
        descriptor = 0x100
        location_length = (0x18A8 << 16) | 12
        for offset, value in ((descriptor, 0x0749), (descriptor + 4, location_length)):
            flash[offset:offset + 4] = value.to_bytes(4, "big")
        return bytes(flash)

    @classmethod
    def build(cls) -> bytearray:
        return make_eeprom_profile.build_profile(cls.firmware_fixture())

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
        image = make_eeprom_profile.build_profile(self.firmware_fixture(), "49015420323751")
        self.assertEqual(image[0x000C:0x0014], bytes.fromhex("4901542032375100"))
        self.assertEqual(image[0x0110:0x0113], bytes.fromhex("123450"))
        self.assertEqual(image[0x06C8:0x06D0], bytes.fromhex("32d8fa9700000317"))
        self.assertEqual(make_eeprom_profile.imei_check_digit("49015420323751"), "8")

    def test_display_profile_uses_rom_descriptor(self):
        flash = bytearray(self.firmware_fixture())
        descriptor = 0x100
        location = 0x18A8
        image = make_eeprom_profile.build_profile(bytes(flash))
        self.assertEqual(image[location:location + 36], bytes.fromhex(
            "000901340104010100ffffff"
            "010801340104ff0100ffffff"
            "000901340104010100ffffff"))

    def test_display_profile_location_can_move_between_roms(self):
        flash = bytearray(self.firmware_fixture())
        location = 0x18A0
        flash[0x104:0x108] = ((location << 16) | 12).to_bytes(4, "big")
        image = make_eeprom_profile.build_profile(bytes(flash))
        self.assertEqual(image[location:location + 9],
                         bytes.fromhex("000901340104010100"))
        self.assertEqual(image[location + 36:location + 45], bytes([0xff]) * 9)


if __name__ == "__main__":
    unittest.main()
