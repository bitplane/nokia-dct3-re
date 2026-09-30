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
        games_location_length = (0x0D9C << 16) | 4
        for offset, value in ((descriptor, 0x0749), (descriptor + 4, location_length),
                              (descriptor + 8, 0x074C), (descriptor + 12, games_location_length),
                              (descriptor + 16, 0x0757),
                              (descriptor + 20, (0x0DB0 << 16) | 400)):
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

    def test_erased_identity_can_have_a_physical_security_code(self):
        image = make_eeprom_profile.build_profile(
            self.firmware_fixture(),
            erased_identity_security_code="12345")
        self.assertEqual(image[0x000C:0x0014], bytes([0xff]) * 8)
        self.assertEqual(image[0x0110:0x0113], bytes.fromhex("123450"))
        self.assertEqual(image[0x06C8:0x06D0], bytes.fromhex(
            "3ad2f490000003c2"))
        make_eeprom_profile.validate_checksums(image)

    def test_display_profile_uses_rom_descriptor(self):
        flash = bytearray(self.firmware_fixture())
        descriptor = 0x100
        location = 0x18A8
        image = make_eeprom_profile.build_profile(bytes(flash))
        self.assertEqual(image[location:location + 36], bytes.fromhex(
            "000901340104010100ffffff"
            "010801340104ff0100ffffff"
            "000901340104010100ffffff"))

    def test_games_records_provision_top_score_and_level(self):
        image = self.build()
        location = 0x0D9C
        self.assertEqual(image[location:location + 20], bytes.fromhex("000000ff") * 5)

    def test_games_records_follow_relocated_descriptor(self):
        flash = bytearray(self.firmware_fixture())
        location = 0x1900
        flash[0x10C:0x110] = ((location << 16) | 4).to_bytes(4, "big")
        flash[0x114:0x118] = (((location + 20) << 16) | 400).to_bytes(4, "big")
        image = make_eeprom_profile.build_profile(bytes(flash))
        self.assertEqual(image[location:location + 20], bytes.fromhex("000000ff") * 5)
        self.assertEqual(image[0x0D9C:0x0DB0], bytes([0xff]) * 20)
        make_eeprom_profile.validate_checksums(image)

    def test_v501_three_games_do_not_overwrite_adjacent_record(self):
        flash = bytearray(self.firmware_fixture())
        flash[0x114:0x118] = ((0x0DA8 << 16) | 400).to_bytes(4, "big")
        image = make_eeprom_profile.build_profile(bytes(flash))
        self.assertEqual(image[0x0D9C:0x0DA8], bytes.fromhex("000000ff") * 3)
        self.assertEqual(image[0x0DA8:0x0DB0], bytes([0xff]) * 8)
        make_eeprom_profile.validate_checksums(image)

    def test_invalid_games_extent_is_rejected(self):
        flash = bytearray(self.firmware_fixture())
        flash[0x114:0x118] = ((0x0DAD << 16) | 400).to_bytes(4, "big")
        with self.assertRaisesRegex(ValueError, "unsupported games NV record extent"):
            make_eeprom_profile.build_profile(bytes(flash))

    def test_acquired_3210_roms_agree_with_game_record_extent(self):
        root = Path(__file__).resolve().parents[1] / "roms" / "noki3210"
        cases = (
            ("3210f600a.fls", 5, 0x29A110, 0x2D9738),
            ("3210f501.fls", 3, 0x2977EC, 0x2D2D4C),
        )
        if not all((root / name).is_file() for name, *_ in cases):
            self.skipTest("optional acquired 3210 ROMs are not installed")
        for name, count, compare, speeds in cases:
            with self.subTest(rom=name):
                flash = (root / name).read_bytes()
                # Thumb big-endian CMP r4,#count at the loader loop tail.
                offset = compare - make_eeprom_profile.FLASH_BASE
                self.assertEqual(flash[offset:offset + 2], bytes((0x2C, count)))
                offset = speeds - make_eeprom_profile.FLASH_BASE
                self.assertEqual(flash[offset:offset + 9],
                                 bytes.fromhex("4230261e17120e0b09"))
                start = make_eeprom_profile.find_nv_descriptor(flash, 0x074C, 4)
                end = make_eeprom_profile.find_nv_descriptor(flash, 0x0757, 400)
                self.assertEqual(end - start, count * 4)
                image = make_eeprom_profile.build_profile(flash)
                self.assertEqual(image[start:end], bytes.fromhex("000000ff") * count)
                make_eeprom_profile.validate_checksums(image)

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
