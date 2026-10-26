#!/usr/bin/env python3
import unittest
import sys
import struct
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import make_eeprom_profile


class ChecksumTests(unittest.TestCase):
    @staticmethod
    def firmware_fixture() -> bytes:
        flash = bytearray(0x1000)
        descriptor = 0x100
        location_length = (0x18A8 << 16) | 12
        games_location_length = (0x0D9C << 16) | 4
        for offset, value in ((descriptor, 0x0749), (descriptor + 4, location_length),
                              (descriptor + 8, 0x074C), (descriptor + 12, games_location_length),
                              (descriptor + 16, 0x0757),
                              (descriptor + 20, (0x0DB0 << 16) | 400)):
            flash[offset:offset + 4] = value.to_bytes(4, "big")
        # A relocated instance of the reviewed variant-zero constructor.
        entry, block = 0x300, 0x356
        struct.pack_into('>4H', flash, entry, 0xb530, 0xb081, 0x20ff, 0x3091)
        struct.pack_into('>4H', flash, block - 0x3e, 0x2500, 0x1c28, 0x2800, 0xd01a)
        words = [0x1c20, 0x4929, 0x2209, 0xf000, 0xf800, 0x4928,
                 0x2009, 0x1900, 0x22ff, 0x3288, 0xf000, 0xf800,
                 0x2000, 0x9000, 0x1c20, 0x4924, 0x4a25, 0x1c2b,
                 0xf000, 0xf800]
        struct.pack_into('>20H', flash, block, *words)
        for offset, value in ((0x400, 0x200800), (0x404, 0x200900), (0x408, 0x757)):
            flash[offset:offset + 4] = value.to_bytes(4, 'big')
        for index in range(9):
            flash[(0x800 + index) ^ 1] = 0xa0 + index
        for index in range(391):
            flash[(0x900 + index) ^ 1] = index & 255
        return bytes(flash)

    @classmethod
    def build(cls) -> bytearray:
        return make_eeprom_profile.build_profile(cls.firmware_fixture())

    def test_profile_has_valid_firmware_checksums(self):
        image = self.build()
        make_eeprom_profile.validate_checksums(image)

    def test_truncated_or_extended_profiles_are_rejected(self):
        image = self.build()
        for invalid in (image[:-1], image + b'\xff', b''):
            with self.assertRaisesRegex(ValueError, 'exactly 16384 bytes'):
                make_eeprom_profile.validate_checksums(invalid)

    def test_display_variants_must_all_fit_the_declared_eeprom(self):
        flash = bytearray(self.firmware_fixture())
        flash[0x104:0x108] = ((0x3ff0 << 16) | 12).to_bytes(4, 'big')
        with self.assertRaisesRegex(ValueError, 'variants extend outside EEPROM'):
            make_eeprom_profile.build_profile(bytes(flash))

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
        layout = make_eeprom_profile.find_fallback_layout(bytes(flash))
        self.assertEqual(image[0x0DA8:0x0DA8 + 400],
                         make_eeprom_profile.fallback_record(bytes(flash), layout))
        make_eeprom_profile.validate_checksums(image)

    def test_fallback_constructor_and_byte_lanes_are_independently_relocated(self):
        flash = self.firmware_fixture()
        layout = make_eeprom_profile.find_fallback_layout(flash)
        self.assertEqual((layout.constructor, layout.header_source, layout.body_source,
                          layout.location), (0x200300, 0x200800, 0x200900, 0xdb0))
        expected = bytes(range(0xa0, 0xa9)) + bytes(i & 255 for i in range(391))
        self.assertEqual(make_eeprom_profile.fallback_record(flash, layout), expected)
        self.assertEqual(self.build()[0xdb0:0xf40], expected)

    def test_fallback_rejects_unknown_body_length_or_variant_selection(self):
        for address, replacement in ((0x35a, b'\x22\x08'), (0x368, b'\x32\x87'),
                                     (0x31e, b'\xd0\x19'), (0x408, b'\x00\x00\x07\x58')):
            flash = bytearray(self.firmware_fixture())
            flash[address:address + len(replacement)] = replacement
            with self.subTest(address=address), self.assertRaisesRegex(ValueError, '0 usable matches'):
                make_eeprom_profile.find_fallback_layout(bytes(flash))

    def test_fallback_payload_outside_flash_is_rejected(self):
        flash = bytearray(self.firmware_fixture())
        flash[0x404:0x408] = (0x200fff).to_bytes(4, 'big')
        with self.assertRaisesRegex(ValueError, 'outside flash'):
            make_eeprom_profile.find_fallback_layout(bytes(flash))

    def test_fallback_ambiguous_constructors_are_rejected(self):
        flash = bytearray(self.firmware_fixture())
        flash[0x500:0x700] = flash[0x300:0x500]
        with self.assertRaisesRegex(ValueError, '2 usable matches'):
            make_eeprom_profile.find_fallback_layout(bytes(flash))

    def test_acquired_roms_select_own_fallback_sources_and_destination(self):
        import hashlib
        root = Path(__file__).resolve().parents[1] / 'roms/noki3210'
        cases = (('3210f600a.fls', 0x26db44, 0x2d7fdc, 0x2d7c08, 0xdb0,
                  '9b648f8b0e3cd056ae58e810dfacf5655e2191471a5dc6e47f030e8dbe32c62a'),
                 ('3210f501.fls', 0x268bd4, 0x2cd368, 0x2ccf94, 0xda8,
                  '608961a98b64a8ae974fb8cb0308d52b4728cbcc581160040af76206f15dc4c1'))
        if not all((root / name).is_file() for name, *_ in cases):
            self.skipTest('optional acquired 3210 ROMs are not installed')
        for name, constructor, header, body, location, profile_hash in cases:
            with self.subTest(rom=name):
                flash = (root / name).read_bytes()
                layout = make_eeprom_profile.find_fallback_layout(flash)
                self.assertEqual((layout.constructor, layout.header_source, layout.body_source,
                                  layout.location), (constructor, header, body, location))
                payload = make_eeprom_profile.fallback_record(flash, layout)
                self.assertEqual(hashlib.sha256(payload).hexdigest(),
                                 '6a4f1801949f15a301f0785057a9e9e8797b9aded70dc751148ef66fdff33a7e')
                image = make_eeprom_profile.build_profile(flash)
                self.assertEqual(image[location:location + 400], payload)
                self.assertEqual(hashlib.sha256(image).hexdigest(), profile_hash)

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
