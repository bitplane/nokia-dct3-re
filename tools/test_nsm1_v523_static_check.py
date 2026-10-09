import unittest
from pathlib import Path

from tools import nsm1_v523_static_check as check


class Nsm1V523StaticCheckTests(unittest.TestCase):
    def test_projection_uses_entire_two_megabyte_source_and_terminators(self):
        image = bytearray(check.SIZE)
        image[0x40:0x42] = b"\x12\x34"
        last = 0x40 + (check.STREAM_WORDS - 1) * 0x20
        image[last:last + 2] = b"\xab\xcd"
        stream = check.extract_stream(bytes(image))
        self.assertEqual(0x20000, len(stream))
        self.assertEqual(b"\x12\x34", stream[:2])
        self.assertEqual(b"\xab\xcd\xff\xff\xff\xff", stream[-6:])
        self.assertEqual(0x3FFFE0, check.BASE + last)

    def test_half_sized_sibling_image_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "complete 2 MiB"):
            check.extract_stream(bytes(0x100000))

    def test_cpu_order_literal_and_bounds(self):
        self.assertEqual(0x12345678, check.read32(bytes.fromhex("12345678"), check.BASE))
        for address in (check.BASE - 1, check.BASE + 1):
            with self.assertRaisesRegex(ValueError, "outside image"):
                check.read32(bytes(4), address)

    def test_thumb_literal_uses_aligned_pc(self):
        image = bytes.fromhex("000048010000000012345678")
        self.assertEqual(0x12345678, check.literal(image, check.BASE + 2))

    def test_arm_literal_uses_pc_plus_eight(self):
        image = bytes.fromhex("e59f00000000000012345678")
        self.assertEqual(0x12345678, check.literal(image, check.BASE, thumb=False))

    def test_non_literal_instruction_rejected(self):
        with self.assertRaisesRegex(ValueError, "not a literal load"):
            check.literal(bytes.fromhex("20000000"), check.BASE)

    def test_wrong_full_image_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "not the normalized"):
            check.verify(bytes(check.SIZE))

    def test_unidentified_package_is_rejected_before_extraction(self):
        with self.assertRaisesRegex(ValueError, "not the acquired"):
            check.normalize_package(b"unknown package")

    def test_unidentified_eeprom_archive_rejected(self):
        with self.assertRaisesRegex(ValueError, "original NokiX"):
            check.extract_eeprom(b"unknown package")

    def test_unidentified_directory_rejected(self):
        with self.assertRaisesRegex(ValueError, "directory fingerprint"):
            check.decode_group7(bytes(check.SIZE))

    def test_original_eeprom_when_available(self):
        archive = Path(__file__).resolve().parents[1] / "roms/research/nsm1-v523/NokiX-scripts-2011.07.24.zip"
        if not archive.exists():
            self.skipTest("original NokiX archive not present")
        eeprom = check.extract_eeprom(archive.read_bytes())
        self.assertEqual(0x4000, len(eeprom))
        self.assertEqual(0x58, eeprom[0x3F3])
        self.assertEqual(bytes.fromhex("3124"), eeprom[0x3D2:0x3D4])

    def test_acquired_package_reproduces_input_when_available(self):
        package = Path(__file__).resolve().parents[1] / "roms/archive-dct3-packages/nsm1_523.exe"
        if not package.exists():
            self.skipTest("acquired NSM-1 package not present")
        result = check.verify(check.normalize_package(package.read_bytes()))
        self.assertEqual(check.SHA1, result["flash_sha1"])

    def test_acquired_image_contract_when_available(self):
        image = Path(__file__).resolve().parents[1] / "roms/research/nsm1-v523/6150-v523-ppm-c.fls"
        if not image.exists():
            self.skipTest("acquired NSM-1 image not present")
        result = check.verify(image.read_bytes())
        self.assertEqual(0x111A94, result["final_publication"]["capture_state"])
        self.assertEqual([0x1160F8] * 3, result["reset_stack_literals"])
        self.assertEqual(127, result["stream"]["full_blocks"])
        self.assertFalse(result["boot_promoted"])
        self.assertFalse(result["final_publication"]["matching_resident_dsp_proven"])
        records = {r["id"]: r for r in result["eeprom"]["records"]}
        self.assertEqual(112, len(records))
        self.assertEqual((0x3CC, 8), (records[0x701]["offset"], records[0x701]["size"]))
        self.assertEqual((0x3D4, 0x2C), (records[0x702]["offset"], records[0x702]["size"]))
        self.assertEqual(0x3F3, result["eeprom"]["security_setting_offset"])
        self.assertEqual(0x3FA8, result["eeprom"]["highest_record_end"])
        self.assertFalse(result["eeprom"]["template_boot_compatibility_proven"])


if __name__ == "__main__":
    unittest.main()
