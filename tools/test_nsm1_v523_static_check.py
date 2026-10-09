import unittest
from pathlib import Path

from tools import nsm1_v523_static_check as check


class Nsm1V523StaticCheckTests(unittest.TestCase):
    def test_own_activation_wrapper_objects_when_available(self):
        image = Path(__file__).resolve().parents[1] / "roms/research/nsm1-v523/6150-v523-ppm-c.fls"
        if not image.exists():
            self.skipTest("acquired NSM-1 input not present")
        data = image.read_bytes()
        wrapper = check.verify(data)["sim_delivery"]["activation_wrapper"]
        self.assertEqual([0x2077E4, 0x207BB0], wrapper["call_candidates"])
        self.assertEqual(0x10E6D6, wrapper["selector"])
        self.assertEqual(1, data[wrapper["normal_object"] - check.BASE + 4])
        self.assertEqual(0x20, data[wrapper["alternate_object"] - check.BASE + 4])
        request = check.verify(data)["sim_delivery"]["application_start_request"]
        self.assertEqual((0x15, 0x2E0B7C, 0x119A),
                         (request["task"], request["object"], request["id"]))
        self.assertEqual([0x21FB5E, 0x21FD14, 0x21FEC6, 0x220574, 0x220B12],
                         request["call_candidates"])
        decoded = check.instruction(data, 0x29F34E)
        self.assertEqual(("bl", "#0x275b60"), (decoded.mnemonic, decoded.op_str))
        for address in request["call_candidates"]:
            decoded = check.instruction(data, address)
            self.assertEqual(("bl", "#0x29f340"), (decoded.mnemonic, decoded.op_str))
        for address in (0x21FCD0, 0x220546, 0x220AF6):
            decoded = check.instruction(data, address)
            self.assertEqual("movs", decoded.mnemonic)
            self.assertTrue(decoded.op_str.endswith("#0x9f"))
        wait = check.verify(data)["sim_delivery"]["lifecycle_wait"]
        self.assertEqual((0x21E41C, 0x1587, 0x10FF08),
                         (wait["entry"], wait["expected_id"], wait["context"]))
        decoded = check.instruction(data, wait["call_site"])
        self.assertEqual(("bl", "#0x21cf0c"), (decoded.mnemonic, decoded.op_str))

    def test_own_descriptor_event_delivery_when_available(self):
        image = Path(__file__).resolve().parents[1] / "roms/research/nsm1-v523/6150-v523-ppm-c.fls"
        if not image.exists():
            self.skipTest("acquired NSM-1 input not present")
        data = image.read_bytes()
        contract = check.verify(data)["sim_delivery"]["descriptor_event"]
        self.assertEqual(0x2D869C, contract["pointer_column"])
        self.assertEqual(0x2D8DB4, contract["pointer_column"] + contract["index"] * contract["stride"])
        self.assertEqual((0x2E0A50, 0x0C), (contract["object"], contract["event"]))
        for address, mnemonic, operands in (
                (0x275FAE, "ldrb", "r2, [r0, #9]"),
                (0x275FB0, "lsls", "r2, r2, #3"),
                (0x275FB2, "ldr", "r1, [r1, r2]"),
                (0x288F7A, "movs", "r0, #0xe3"),
                (0x288F7C, "movs", "r1, #0xff"),
                (0x288F7E, "adds", "r1, #0x79"),
                (0x288F80, "bl", "#0x275106")):
            decoded = check.instruction(data, address)
            self.assertEqual((mnemonic, operands), (decoded.mnemonic, decoded.op_str))
        self.assertFalse(contract["runtime_schedule_verified"])

    def test_direct_call_candidates_decode_thumb_big_endian_and_bounds(self):
        self.assertEqual([check.BASE],
                         check.direct_call_candidates(bytes.fromhex("f000f800"), check.BASE + 4))
        self.assertEqual([], check.direct_call_candidates(bytes.fromhex("f000f800"), check.BASE + 8))
        self.assertEqual([], check.direct_call_candidates(bytes.fromhex("f000f8"), check.BASE + 4))

    def test_readiness_selector_and_all_three_inputs(self):
        for selector in (0, 1, 0xFF):
            for first in (0, 1):
                for second in (0, 1, 2):
                    for third in (0, 1):
                        expected = bool(first and third and
                                        (second == 1 if selector == 0 else second != 0))
                        self.assertEqual(expected, check.readiness(first, selector, second, third))
        self.assertFalse(check.readiness(1, 0xFF, 0, 1))
        with self.assertRaisesRegex(ValueError, "must be bytes"):
            check.readiness(1, 256, 1, 1)

    def test_own_simi_register_contract_when_available(self):
        image = Path(__file__).resolve().parents[1] / "roms/research/nsm1-v523/6150-v523-ppm-c.fls"
        if not image.exists():
            self.skipTest("acquired NSM-1 input not present")
        contract = check.verify(image.read_bytes())["simi"]
        self.assertEqual((0x20037, 0x2003C),
                         (contract["rx_data"], contract["rx_count"]))
        self.assertEqual((0x20038, 0x20039),
                         (contract["interrupt_cause"], contract["control"]))
        self.assertEqual(0x10E6CE,
                         contract["readiness_object"] + contract["state_offset"])
        self.assertEqual(0x288FB2, contract["retry_state_store"])

    def test_own_serial_reader_contract_when_available(self):
        image = Path(__file__).resolve().parents[1] / "roms/research/nsm1-v523/6150-v523-ppm-c.fls"
        if not image.exists():
            self.skipTest("acquired NSM-1 input not present")
        contract = check.verify(image.read_bytes())["eeprom"]["serial_receive"]
        self.assertEqual(0x20020, contract["gpio_data_address"])
        self.assertEqual(0x20024, contract["gpio_direction_address"])
        self.assertEqual((0, 2), (contract["sda_bit"], contract["scl_bit"]))
        self.assertTrue(contract["release_sda_clears_direction_bit"])
        self.assertTrue(contract["sample_on_scl_high"])
        self.assertTrue(contract["msb_first"])
        self.assertFalse(contract["runtime_verified"])

    def test_own_gensio_read_contract_when_available(self):
        image = Path(__file__).resolve().parents[1] / "roms/research/nsm1-v523/6150-v523-ppm-c.fls"
        if not image.exists():
            self.skipTest("acquired NSM-1 input not present")
        contract = check.verify(image.read_bytes())["gensio_read"]
        self.assertEqual({"routine": 0x2BF082, "base": 0x20000,
                          "control": 0x28, "selection": 0x22,
                          "tx": 0x2A, "status": 0x29,
                          "ready_bit": 2, "rx": 0x2D}, contract)

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

    def test_identity_formatter_uses_high_then_low_bcd_and_derived_digit(self):
        eeprom = bytearray(0x4000)
        eeprom[0x0C:0x13] = bytes.fromhex("49015420323751")
        eeprom[0x13] = 0xFF
        self.assertEqual(b"490154203237518\0", check.identity_buffer(bytes(eeprom)))

    def test_identity_formatter_rejects_short_or_non_decimal_input(self):
        with self.assertRaisesRegex(ValueError, "16 KiB"):
            check.identity_buffer(bytes(8))
        eeprom = bytearray(0x4000)
        eeprom[0x0C] = 0xFA
        with self.assertRaisesRegex(ValueError, "non-decimal"):
            check.identity_buffer(bytes(eeprom))

    def test_security_fixture_only_changes_loaded_checksum(self):
        root = Path(__file__).resolve().parents[1]
        image = root / "roms/research/nsm1-v523/6150-v523-ppm-c.fls"
        archive = root / "roms/research/nsm1-v523/NokiX-scripts-2011.07.24.zip"
        if not image.exists() or not archive.exists():
            self.skipTest("acquired NSM-1 inputs not present")
        contract = check.verify(image.read_bytes())["eeprom"]
        original = check.extract_eeprom(archive.read_bytes())
        audit = check.audit_security(original, contract)
        self.assertEqual(b"493006102132132\0".hex(), audit["identity_buffer_hex"])
        self.assertEqual(0x34D, audit["expected_security_checksum"])
        self.assertFalse(audit["checksum_valid"])
        fixture = check.security_fixture(original, contract)
        self.assertEqual([0x3D2, 0x3D3],
                         [i for i, (a, b) in enumerate(zip(original, fixture)) if a != b])
        self.assertTrue(check.audit_security(fixture, contract)["checksum_valid"])
        self.assertEqual(original[0x3F3], fixture[0x3F3])
        self.assertEqual(original[0x0C:0x14], fixture[0x0C:0x14])

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
