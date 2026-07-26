import struct
import unittest

from tools import nse3_v406_static_check as check


class Nse3V406StaticCheckTests(unittest.TestCase):
    def fixture(self):
        image = bytearray(0x180)
        words = {
            0x40: "e3a01602",
            0x44: "e5910000",
            0x58: "e3a01701",
            0x5C: "e5810000",
            0xB8: "e59f00a8",
            0xBC: "e3a01000",
            0xC0: "e89003fc",
            0xC4: "e88103fc",
            0xE4: "e28f0001",
            0xE8: "e12fff10",
        }
        for offset, word in words.items():
            image[offset : offset + 4] = bytes.fromhex(word)
        for offset, value in {
            0x168: check.VECTOR_SOURCE,
            0x16C: 0x10F9D8,
            0x170: 0x10F9D8,
            0x174: 0x100020,
            0x178: 0x10C508,
        }.items():
            struct.pack_into(">I", image, offset, value)
        return image

    def test_accepts_reset_vector_copy_and_sram_stacks(self):
        result = check.verify_reset_boundary(bytes(self.fixture()))
        self.assertEqual(0, result["vector_destination"])
        self.assertEqual(check.VECTOR_SOURCE, result["literals"]["vector_source"])

    def test_rejects_stack_outside_physical_sram(self):
        image = self.fixture()
        struct.pack_into(">I", image, 0x178, check.SRAM_BASE + check.SRAM_SIZE)
        with self.assertRaisesRegex(ValueError, "outside 64 KiB"):
            check.verify_reset_boundary(bytes(image))

    def test_swap16_is_involutive(self):
        value = bytes.fromhex("12345678")
        self.assertEqual(value, check.swap16(check.swap16(value)))

    def test_identity_rejects_unidentified_image(self):
        with self.assertRaisesRegex(ValueError, "expected 0x100000-byte"):
            check.verify_identity(b"\xff" * 32)

    def test_keypad_boundary_distinguishes_side_keys(self):
        image = bytearray(b"\xff" * check.FLASH_SIZE)
        offset = check.KEYMAP_ADDRESS - check.FLASH_BASE
        special_offset = check.SPECIAL_KEYMAP_ADDRESS - check.FLASH_BASE
        image[offset : offset + len(check.KEYMAP)] = check.KEYMAP
        image[special_offset : special_offset + len(check.SPECIAL_KEYMAP)] = (
            check.SPECIAL_KEYMAP
        )
        result = check.verify_keypad_boundary(bytes(image))
        self.assertEqual(0x11, result["volume_down"]["keycode"])
        self.assertEqual(0x10, result["volume_up"]["keycode"])

    def test_keypad_boundary_rejects_reversed_volume_key(self):
        image = bytearray(b"\xff" * check.FLASH_SIZE)
        offset = check.KEYMAP_ADDRESS - check.FLASH_BASE
        image[offset : offset + len(check.KEYMAP)] = check.KEYMAP
        image[offset + 5] = 0x10
        with self.assertRaisesRegex(ValueError, "normal 5x5 keypad"):
            check.verify_keypad_boundary(bytes(image))

    def test_eeprom_boundary_recovers_sda_scl_and_address_width(self):
        image = bytearray(b"\xff" * check.FLASH_SIZE)
        # swap16 Thumb encodings at the identified routine anchors.
        encodings = {
            0x29CDD2: "380a",
            0x29CDE0: "3806",
            0x29CDE2: "000e",
            0x29E8C4: "8024",
            0x29E8C8: "2033",
            0x29E8CA: "0120",
            0x29E8CC: "0422",
            0x29E8D4: "1979",
            0x29E90E: "1543",
            0x29E936: "9543",
        }
        for pc, encoded in encodings.items():
            physical = bytes.fromhex(encoded)
            offset = pc - check.FLASH_BASE
            image[offset : offset + 2] = physical[::-1]
        result = check.verify_eeprom_boundary(bytes(image))
        self.assertEqual(0, result["sda_bit"])
        self.assertEqual(2, result["scl_bit"])
        self.assertEqual(2, result["word_address_bytes"])

    def test_simi_boundary_recovers_atr_pps_without_assigning_subscriber_identity(self):
        image = bytearray(b"\xff" * check.FLASH_SIZE)
        encodings = {
            0x290510: "2020", 0x290512: "617b",
            0x29051C: "3d21", 0x29051E: "1820",
            0x29052A: "3e21", 0x29052C: "1a20",
            0x290538: "3920", 0x29053A: "3221",
            0x2900FA: "8020", 0x2901B0: "4b71",
            0x2901B4: "0b72", 0x2901C2: "0b70",
            0x2901D0: "0d72", 0x2903DE: "6879",
            0x2903E4: "2878", 0x290462: "0878",
            0x29046A: "0870",
            0x275240: "6878", 0x275246: "3b21",
            0x275248: "401a", 0x27524E: "0438",
            0x275254: "aa78", 0x275260: "5309",
            0x27527C: "5208", 0x2752C2: "6146",
            0x2752C4: "8842", 0x2752D2: "6846",
            0x2752D4: "8078", 0x2752D6: "1121",
            0x2752DE: "8338", 0x2752E4: "0120",
            0x2752E8: "ff20", 0x2752EA: "2876",
            0x2752EC: "6e76", 0x2752EE: "a876",
            0x2752F2: "0220", 0x2752FC: "2876",
            0x2752FE: "1020", 0x275300: "6876",
            0x275302: "9420", 0x275304: "a876",
            0x275306: "7b20", 0x275308: "e876",
            0x275C14: "fff7f9fa",
            0x2758F4: "4878", 0x2758FA: "8b78",
            0x275900: "9228", 0x275906: "6721",
            0x27590E: "0838", 0x275914: "2138",
            0x27591A: "0138", 0x275922: "9321",
            0x27592A: "0138", 0x275930: "0438",
            0x275936: "0738", 0x275954: "981e",
            0x27595A: "0238", 0x275960: "0438",
            0x275966: "0838", 0x27596C: "3038",
            0x275972: "1038",
        }
        for pc, encoded in encodings.items():
            physical = bytes.fromhex(encoded)
            offset = pc - check.FLASH_BASE
            for index in range(0, len(physical), 2):
                image[offset + index : offset + index + 2] = physical[index : index + 2][::-1]
        result = check.verify_simi_boundary(bytes(image))
        self.assertEqual(0x20036, result["tx_data"])
        self.assertEqual(0x20037, result["rx_data"])
        self.assertEqual([0x3B, 0x10, 0x05], result["lab_card_atr"])
        self.assertEqual([0xFF, 0x00, 0xFF], result["pps_contract"]["ordinary_ta1"])
        self.assertTrue(result["lab_card_atr_pps_compatible"])
        self.assertEqual(
            [0x67, 0x90, 0x94, 0x98, 0x9F],
            result["lab_card_explicit_status_families"],
        )
        self.assertEqual(
            "generic_command_error",
            result["unsupported_instruction_status"]["firmware_path"],
        )
        self.assertEqual(
            "removable_lab_fixture_not_nse3_identity",
            result["subscriber_filesystem_profile"],
        )

    def test_sim_apdu_constructor_vocabulary_is_product_evidence(self):
        self.assertEqual(
            [
                0x10, 0x12, 0x14, 0x20, 0x24, 0x26, 0x28, 0x2C, 0x32,
                0x88, 0xA4, 0xB0, 0xB2, 0xC0, 0xC2, 0xD6, 0xDC, 0xF2,
            ],
            sorted(
                {
                    constructor["instruction"]
                    for constructor in check.SIM_APDU_CONSTRUCTORS
                }
            ),
        )
        self.assertEqual(2, sum(
            constructor["instruction"] == 0xA4
            for constructor in check.SIM_APDU_CONSTRUCTORS
        ))

    def test_dspif_boundary_recovers_ring_geometry_without_reply_semantics(self):
        physical = bytearray(b"\xff" * check.FLASH_SIZE)
        encodings = {
            0x28564C: "ee49", 0x28564E: "0420", 0x285650: "0880",
            0x2856D2: "d34b", 0x2856D4: "a422",
            0x2856E6: "8d42", 0x2856EA: "1d1c", 0x28572A: "1080",
            0x28577A: "f048", 0x28577E: "f048", 0x28578C: "6430",
            0x2857C8: "de4c", 0x2857F6: "3152",
            0x2858A0: "a948", 0x2858A2: "0288", 0x2858A4: "4188",
            0x285A42: "4148", 0x285A44: "0180",
            0x285A46: "3e4b", 0x285A48: "8022",
            0x285A4A: "1a80", 0x285A4C: "5a80", 0x285A4E: "4180",
        }
        for pc, encoded in encodings.items():
            offset = pc - check.FLASH_BASE
            physical[offset : offset + 2] = bytes.fromhex(encoded)
        pools = {
            0x285A08: 0x30000,
            0x285A20: 0x10000,
            0x285B3C: 0x101CA,
            0x285B40: 0x101C8,
            0x285B44: 0x10100,
            0x285B48: 0x100A4,
        }
        for address, value in pools.items():
            raw = ((value << 16) | (value >> 16)) & 0xFFFFFFFF
            struct.pack_into("<I", physical, address - check.FLASH_BASE, raw)
        result = check.verify_dspif_boundary(check.swap16(bytes(physical)))
        self.assertEqual(0x0A4, result["mcu_to_dsp"]["producer_byte"])
        self.assertEqual(0x1CA, result["dsp_to_mcu"]["consumer_byte"])
        self.assertEqual("missing", result["internal_dsp_image"])
        self.assertEqual(
            "not_established", result["bootstrap_reply_semantics"]
        )

    def test_dspif_boundary_rejects_inherited_cursor_address(self):
        physical = bytearray(b"\xff" * check.FLASH_SIZE)
        # A syntactically valid LDR at the first literal anchor must not be
        # enough when its effective pointer is from another product.
        offset = 0x28564C - check.FLASH_BASE
        physical[offset : offset + 2] = bytes.fromhex("ee49")
        wrong = 0x101EC
        raw = ((wrong << 16) | (wrong >> 16)) & 0xFFFFFFFF
        struct.pack_into("<I", physical, 0x285A08 - check.FLASH_BASE, raw)
        with self.assertRaisesRegex(ValueError, "Thumb literal"):
            check.verify_thumb_literals(
                check.swap16(bytes(physical)), {0x28564C: 0x30000}
            )

    def test_radio_report_routes_keep_task_11_and_task_12_distinct(self):
        self.assertEqual(
            [
                {"task": 11, "status": 0x139F},
                {"task": 12, "status": 0x13A0},
            ],
            check.FIXED_RADIO_TASK_ROUTES["0x83"]["routes"],
        )
        self.assertEqual(
            {"task": 12, "status": 0x13B8, "object_bytes": 0xA8},
            check.FIXED_RADIO_TASK_ROUTES["0x8b"],
        )
        self.assertEqual(
            [0x67, 0x72, 0x71, 0x68],
            [
                route["code"]
                for route in
                check.TASK_11_CONTROLLER_DISPATCH["fixed_control_calls"].values()
            ],
        )
        self.assertFalse(
            check.TASK_11_CONTROLLER_DISPATCH["semantic_names_assigned"]
        )
        self.assertEqual(
            0x21718C, check.TASK_12_STATUS_JUMP_TABLE[0x13B8]
        )
        self.assertEqual(
            0x21732E, check.TASK_12_STATUS_JUMP_TABLE[0x13AA]
        )
        self.assertNotEqual(
            check.TASK_12_STATUS_JUMP_TABLE[0x13B8],
            check.TASK_12_STATUS_JUMP_TABLE[0x13AA],
        )
        self.assertEqual(0x04E6, check.RADIO_REPORT_HANDLER_LITERALS[0x21409A])
        self.assertEqual(0x0EB4, check.RADIO_REPORT_HANDLER_LITERALS[0x2140B0])
        self.assertEqual(
            bytes.fromhex("030c0000002be7b0"),
            check.MEASUREMENT_TIMER_CONFIGURATION,
        )
        self.assertEqual(
            bytes.fromhex("13aa0000"), check.MEASUREMENT_TIMER_EVENT_PREFIX
        )
        self.assertEqual(
            {
                "task": 12,
                "status": 0x139E,
                "object_bytes": 0x40,
                "copied_report_bytes": 0x18,
                "conditional_route": True,
            },
            check.FIXED_RADIO_TASK_ROUTES["0x80"],
        )
        self.assertEqual(
            0x106B0D, check.RADIO_REPORT_HANDLER_LITERALS[0x2171CE]
        )
        self.assertEqual(
            0x106B08, check.RADIO_REPORT_HANDLER_LITERALS[0x2171F0]
        )

    def test_bitmap_constructors_keep_populated_and_zero_control_forms_distinct(self):
        self.assertEqual(0x09CD, check.RADIO_PACKET_LITERALS[0x20FD94])
        self.assertEqual(0x0445, check.RADIO_PACKET_LITERALS[0x24E74A])
        self.assertEqual(
            ("movs", "r2, #0x48"), check.RADIO_PACKET_ANCHORS[0x216F7E]
        )
        self.assertEqual(
            ("orrs", "r0, r1"), check.RADIO_PACKET_ANCHORS[0x216F98]
        )
        self.assertEqual(
            bytes.fromhex("03040000000000db"),
            check.SEARCH_SUBMISSION_TIMER_CONFIGURATION,
        )
        self.assertEqual(0x2B74E0, check.NSE3_TASK_4_ENTRY_POINTER)
        self.assertEqual(0x2A20DD, check.NSE3_TASK_4_ENTRY)
        self.assertEqual(
            ("cmp", "r4, #0xdb"), check.RADIO_PACKET_ANCHORS[0x2A2228]
        )

    def test_channel_configure_caller_profiles_preserve_distinct_contexts(self):
        self.assertEqual(
            [0x20D1C2, 0x20DA60, 0x20EA06, 0x210A0E, 0x210CAA],
            check.CHANNEL_CONFIGURE_DIRECT_CALLS,
        )
        self.assertEqual(
            [[4, 6, 7], [4], [4], [4], [4]],
            [
                profile["operation_values"]
                for profile in check.CHANNEL_CONFIGURE_CALLER_PROFILES
            ],
        )
        self.assertEqual(
            [0x10, 0x1A, 0x1A, "table_0x2bd710", 0x50],
            [
                profile["constructor_argument"]
                for profile in check.CHANNEL_CONFIGURE_CALLER_PROFILES
            ],
        )
        self.assertTrue(
            all(
                profile["destination_task"] == 3
                for profile in check.CHANNEL_CONFIGURE_CALLER_PROFILES
            )
        )
        self.assertEqual(0x2BD710, check.RADIO_PACKET_LITERALS[0x210A08])
        self.assertEqual(
            ("bl", "#0x211550"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x211DCC],
        )

    def test_type_0x8a_uses_shared_controller_state_not_report_body(self):
        self.assertEqual(
            0x1090FC, check.RADIO_REPORT_HANDLER_LITERALS[0x2803FC]
        )
        self.assertEqual(
            ("cmp", "r1, #1"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x280400],
        )
        self.assertEqual(
            ("movs", "r1, #0xfd"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x280412],
        )
        self.assertEqual(
            ("b", "#0x211e48"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x211C9A],
        )
        self.assertEqual(
            0x10FECC, check.RADIO_REPORT_HANDLER_LITERALS[0x211E56]
        )
        self.assertEqual(
            0x109178, check.RADIO_REPORT_HANDLER_LITERALS[0x211E6A]
        )
        self.assertEqual(
            ("cmp", "r0, #6"), check.RADIO_PACKET_ANCHORS[0x20E9DA]
        )
        self.assertEqual(
            ("cmp", "r0, #7"), check.RADIO_PACKET_ANCHORS[0x20E9E2]
        )

    def test_task_11_decoder_keeps_type_0x87_and_0x8a_paths_distinct(self):
        self.assertEqual(0x210DA8, check.TASK_11_EVENT_DECODER)
        self.assertEqual(
            [0x211CFC, 0x211D1A, 0x211E66, 0x211E8A, 0x211E98, 0x211ED8],
            check.TASK_11_EVENT_DECODER_DIRECT_CALLS,
        )
        self.assertEqual(
            0x2112A4, check.TASK_11_EVENT_JUMP_TABLE[0x138F]
        )
        self.assertEqual(
            0x211288, check.TASK_11_EVENT_JUMP_TABLE[0x1390]
        )
        self.assertEqual(
            ("bl", "#0x20faec"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x211360],
        )
        self.assertEqual(
            ("bl", "#0x20fa18"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x2112B8],
        )
        self.assertEqual(
            ("bl", "#0x25ef90"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x280452],
        )
        self.assertEqual(
            0x109178, check.RADIO_REPORT_HANDLER_LITERALS[0x280432]
        )

    def test_type_0x83_routes_scalar_report_by_controller_state(self):
        self.assertEqual(
            0x1090FF, check.RADIO_REPORT_HANDLER_LITERALS[0x27FD44]
        )
        self.assertEqual(
            ("cmp", "r0, #3"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x27FD48],
        )
        self.assertEqual(
            ("cmp", "r0, #1"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x27FD4C],
        )
        self.assertEqual(
            0x139F, check.RADIO_REPORT_HANDLER_LITERALS[0x210E0A]
        )
        self.assertEqual(
            0x10918D, check.RADIO_REPORT_HANDLER_LITERALS[0x210E14]
        )
        self.assertEqual(
            ("b", "#0x2173b0"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x217092],
        )
        self.assertEqual(
            ("bl", "#0x21630c"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x2173B2],
        )
        self.assertEqual(
            ("movs", "r0, #0x6c"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x2163D0],
        )

    def test_scalar_measurement_timer_is_distinct_one_shot_control(self):
        self.assertEqual(0x6C, check.SCALAR_MEASUREMENT_TIMER_CODE)
        self.assertEqual(
            bytes.fromhex("010c0000002be7a8"),
            check.SCALAR_MEASUREMENT_TIMER_CONFIGURATION,
        )
        self.assertEqual(
            bytes.fromhex("13a60000"),
            check.SCALAR_MEASUREMENT_TIMER_EVENT_PREFIX,
        )
        self.assertEqual(
            [0x2163DE], check.SCALAR_MEASUREMENT_TIMER_ARM_CALLS
        )
        self.assertEqual(
            [0x2156BC, 0x21594C, 0x2163F2],
            check.SCALAR_MEASUREMENT_TIMER_CANCEL_CALLS,
        )
        self.assertEqual(
            [0x2156B0, 0x2163D2, 0x2163E8],
            check.SCALAR_MEASUREMENT_TIMER_QUERY_CALLS,
        )
        self.assertEqual(
            0x0273, check.RADIO_REPORT_HANDLER_LITERALS[0x2163DC]
        )
        self.assertEqual(
            ("b", "#0x2156e4"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x2155D6],
        )
        self.assertEqual(
            0x13A5, check.RADIO_REPORT_HANDLER_LITERALS[0x2155C6]
        )
        self.assertEqual(
            0x106CEA, check.RADIO_REPORT_HANDLER_LITERALS[0x2156EC]
        )

    def test_type_0x88_repacks_timing_fields_for_candidate_control(self):
        self.assertEqual(
            0x1090FF, check.RADIO_REPORT_HANDLER_LITERALS[0x280468]
        )
        self.assertEqual(
            0x13AC, check.RADIO_REPORT_HANDLER_LITERALS[0x280478]
        )
        self.assertEqual(
            ("str", "r0, [r5, #4]"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x28048A],
        )
        self.assertEqual(
            ("strh", "r0, [r5, #8]"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x280494],
        )
        self.assertEqual(
            ("strh", "r0, [r5, #0xa]"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x28049E],
        )
        self.assertEqual(
            0x106B08, check.RADIO_REPORT_HANDLER_LITERALS[0x2172D4]
        )
        self.assertEqual(
            0x106AF6, check.RADIO_REPORT_HANDLER_LITERALS[0x2172DC]
        )
        self.assertEqual(
            ("bl", "#0x2142b2"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x21716E],
        )
        self.assertEqual(
            ("bl", "#0x2159b0"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x217328],
        )
        self.assertEqual(
            ("adds", "r4, #0x18"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x215A18],
        )

    def test_type_0x8f_triggers_distinct_candidate_list_request_stage(self):
        self.assertEqual(
            0x1090FF, check.RADIO_REPORT_HANDLER_LITERALS[0x2803CC]
        )
        self.assertEqual(
            0x13B7, check.RADIO_REPORT_HANDLER_LITERALS[0x2803DC]
        )
        self.assertEqual(0x214788, check.CANDIDATE_LIST_REQUEST_BUILDER)
        self.assertEqual(
            [0x216192, 0x217210],
            check.CANDIDATE_LIST_REQUEST_BUILDER_DIRECT_CALLS,
        )
        self.assertEqual(
            ("movs", "r0, #0x54"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x2147A2],
        )
        self.assertEqual(
            ("adds", "r0, #0x18"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x2147D8],
        )
        self.assertEqual(
            ("cmp", "r2, #0x28"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x2147E4],
        )
        self.assertEqual(
            ("movs", "r0, #0x50"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x21480A],
        )
        self.assertEqual(
            ("movs", "r0, #9"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x21480E],
        )
        self.assertEqual(
            ("bl", "#0x25fb4c"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x214814],
        )
        self.assertEqual(
            ("movs", "r0, #0x6e"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x217218],
        )

    def test_type_0x8c_fixed_notification_is_not_assumed_type_0x09_ack(self):
        self.assertEqual(
            bytes.fromhex("13950000"), check.TYPE_0X8C_FIXED_OBJECT
        )
        self.assertEqual(
            0x1090FA, check.RADIO_REPORT_HANDLER_LITERALS[0x2804C6]
        )
        self.assertEqual(
            0x2BCFA8, check.RADIO_REPORT_HANDLER_LITERALS[0x2804D4]
        )
        self.assertEqual(
            ("cmp", "r1, #1"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x2804CA],
        )
        self.assertEqual(
            ("cmp", "r0, #0x1a"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x2804D0],
        )
        self.assertEqual(
            ("movs", "r0, #0x11"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x211D6A],
        )
        self.assertEqual(
            ("bl", "#0x210da8"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x211ED8],
        )
        self.assertEqual(
            0x13A5, check.RADIO_REPORT_HANDLER_LITERALS[0x21138E]
        )

    def test_timer_0x6e_joins_distinct_paths_as_controller_recheck(self):
        self.assertEqual(0x6E, check.CONTROLLER_RECHECK_TIMER_CODE)
        self.assertEqual(
            bytes.fromhex("010c0000002be7b4"),
            check.CONTROLLER_RECHECK_TIMER_CONFIGURATION,
        )
        self.assertEqual(
            bytes.fromhex("13b00000"),
            check.CONTROLLER_RECHECK_TIMER_EVENT_PREFIX,
        )
        self.assertEqual(
            [0x2141E6, 0x214320, 0x21721C],
            check.CONTROLLER_RECHECK_TIMER_ARM_CALLS,
        )
        self.assertEqual(
            [0x212092], check.CONTROLLER_RECHECK_TIMER_CANCEL_CALLS
        )
        self.assertEqual([], check.CONTROLLER_RECHECK_TIMER_QUERY_CALLS)
        self.assertEqual(
            ("movs", "r1, #2"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x217262],
        )
        self.assertEqual(
            ("cmp", "r1, #0x1a"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x217270],
        )
        self.assertEqual(
            ("bl", "#0x212088"),
            check.RADIO_REPORT_HANDLER_ANCHORS[0x21740E],
        )
        self.assertEqual(
            0x297001, check.RADIO_REPORT_HANDLER_LITERALS[0x21730A]
        )

    def test_external_service_transport_is_shared_without_inheriting_application_script(self):
        self.assertEqual(0x2B751C, check.NSE3_TASK_9_ENTRY_POINTER)
        self.assertEqual(0x273B2D, check.NSE3_TASK_9_ENTRY)
        self.assertEqual(
            ("cmp", "r1, #0x8e"),
            check.EXTERNAL_SERVICE_TRANSPORT_ANCHORS[0x273B46],
        )
        self.assertEqual(
            ("movs", "r0, #0xd0"),
            check.EXTERNAL_SERVICE_TRANSPORT_ANCHORS[0x273896],
        )
        self.assertEqual(
            ("movs", "r0, #0x1e"),
            check.EXTERNAL_SERVICE_TRANSPORT_ANCHORS[0x2738D8],
        )
        self.assertEqual(
            ("movs", "r1, #0x70"),
            check.EXTERNAL_SERVICE_APPLICATION_ANCHORS[0x239900],
        )
        self.assertEqual(
            ("cmp", "r1, #0x42"),
            check.EXTERNAL_SERVICE_APPLICATION_ANCHORS[0x2398A4],
        )
        self.assertEqual(
            ("movs", "r1, #0x40"),
            check.EXTERNAL_SERVICE_APPLICATION_ANCHORS[0x2398B2],
        )
        self.assertEqual(
            ("bl", "#0x293a40"),
            check.EXTERNAL_SERVICE_APPLICATION_ANCHORS[0x2398B4],
        )
        self.assertEqual(
            ("movs", "r1, #0x64"),
            check.EXTERNAL_SERVICE_APPLICATION_ANCHORS[0x239D04],
        )
        self.assertEqual(
            ("movs", "r0, #0x30"),
            check.EXTERNAL_SERVICE_APPLICATION_ANCHORS[0x239D22],
        )

    def test_external_service_controller_events_and_thresholds_are_product_evidence(self):
        self.assertEqual(
            ("bl", "#0x273ab4"),
            check.EXTERNAL_SERVICE_CONTROLLER_ANCHORS[0x273BF2],
        )
        self.assertEqual(
            ("bl", "#0x273a48"),
            check.EXTERNAL_SERVICE_CONTROLLER_ANCHORS[0x273BEC],
        )
        self.assertEqual(
            ("cmp", "r1, #0x5a"),
            check.EXTERNAL_SERVICE_CONTROLLER_ANCHORS[0x273B80],
        )
        self.assertEqual(
            ("cmp", "r1, #0xe6"),
            check.EXTERNAL_SERVICE_CONTROLLER_ANCHORS[0x273B92],
        )
        self.assertEqual(
            ("cmp", "r0, #0xa"),
            check.EXTERNAL_SERVICE_CONTROLLER_ANCHORS[0x273A70],
        )
        self.assertEqual(
            ("cmp", "r1, #2"),
            check.EXTERNAL_SERVICE_CONTROLLER_ANCHORS[0x273AD2],
        )

    def test_external_service_application_dispatch_separates_result_and_control(self):
        self.assertEqual(
            ("cmp", "r2, #0x64"),
            check.EXTERNAL_SERVICE_APPLICATION_DISPATCH_ANCHORS[0x239F02],
        )
        self.assertEqual(
            ("ldrb", "r0, [r4, #9]"),
            check.EXTERNAL_SERVICE_APPLICATION_DISPATCH_ANCHORS[0x23A38C],
        )
        self.assertEqual(
            ("cmp", "r2, #0x70"),
            check.EXTERNAL_SERVICE_APPLICATION_DISPATCH_ANCHORS[0x239F84],
        )
        self.assertEqual(
            ("movs", "r1, #0xd3"),
            check.EXTERNAL_SERVICE_APPLICATION_DISPATCH_ANCHORS[0x23A3C6],
        )
        self.assertEqual(
            ("movs", "r0, #2"),
            check.EXTERNAL_SERVICE_APPLICATION_DISPATCH_ANCHORS[0x23A59E],
        )

    def test_dsp_bootstrap_stream_preserves_stride_extent_and_terminators(self):
        image = bytearray(b"\xff" * check.FLASH_SIZE)
        image[0x40:0x42] = b"\x12\x34"
        image[0x60:0x62] = b"\x56\x78"
        last = 0x2FFFE0 - check.FLASH_BASE
        image[last : last + 2] = b"\x9a\xbc"
        stream = check.extract_dsp_bootstrap_stream(bytes(image))
        self.assertEqual(0x10000, len(stream))
        self.assertEqual(b"\x12\x34\x56\x78", stream[:4])
        self.assertEqual(b"\x9a\xbc\xff\xff\xff\xff", stream[-6:])
        self.assertEqual(
            [
                {"address": 0x100F6, "value": 0x0100},
                {"address": 0x100F8, "value": 0x0300},
                {"address": 0x100FA, "value": 0x0000},
                {"address": 0x100FC, "value": 0x8000},
                {"address": 0x100FE, "value": 0x0001},
                {"address": 0x10100, "value": 0x0001},
                {"address": 0x10102, "value": 0x0200},
            ],
            check.DSP_BOOTSTRAP_HEADER,
        )

    def test_selector_8_state_boundary_keeps_live_and_shadow_blocks_distinct(self):
        self.assertNotEqual(0x10C020, 0x10C008)
        self.assertEqual(
            [0x2B8608, 0x2B861C],
            [check.DSP_PARAMETER_08_LITERALS[address]
             for address in (0x2837BE, 0x283AC4)],
        )
        self.assertEqual(
            [0x2B85F8, 0x2B860C],
            [check.DSP_PARAMETER_08_LITERALS[address]
             for address in (0x2837D8, 0x283AE8)],
        )
        self.assertEqual(0x076F, check.DSP_PARAMETER_EVENT_BASE)
        self.assertEqual(10, len(check.DSP_PARAMETER_EVENT_TARGETS))
        self.assertEqual(
            [0x259442, 0x259442],
            check.DSP_PARAMETER_EVENT_TARGETS[6:8],
        )
        self.assertEqual(
            [0x25A87E],
            check.DSP_PARAMETER_UNRESOLVED_EVENT_CALLS,
        )
        self.assertEqual(
            0x2B43F0,
            check.DSP_PARAMETER_MODE_EVENT_TABLE_ADDRESS,
        )
        self.assertEqual(
            0x1061A4,
            check.DSP_PARAMETER_RUNTIME_RECORD_TABLE_ADDRESS,
        )
        self.assertEqual(
            0x106A64,
            check.DSP_PARAMETER_OBJECT_GROUP_TABLE_ADDRESS,
        )
        self.assertEqual(
            0x10B284,
            check.DSP_PARAMETER_RUNTIME_OBJECT_CELL_ADDRESS,
        )
        self.assertEqual(
            [0x27C17C],
            check.DSP_PARAMETER_UNRESOLVED_OBJECT_CONSTRUCTORS,
        )
        self.assertEqual(0x0389, check.DSP_PARAMETER_RUNTIME_OBJECT_EVENT)
        self.assertEqual(
            {
                0x231660: [0x0000, 0x0000],
                0x25A8FA: [None, None],
                0x25B044: [None, 0x0000],
            },
            check.DSP_PARAMETER_RUNTIME_OBJECT_EVENT_PRODUCERS,
        )
        self.assertEqual(0x25A7D0, check.DSP_PARAMETER_OBJECT_EMITTER_ADDRESS)
        self.assertEqual(
            {
                0x25ADA4: 0x00FF,
                0x25AE8A: 0x00FF,
                0x25B076: 0x00FF,
                0x25B0B2: 0x00FF,
            },
            check.DSP_PARAMETER_OBJECT_EMITTER_CALLS,
        )
        self.assertEqual(
            {
                0x231660: [0x0000, 0x0000],
                0x25A8FA: [None, 0x00FF],
                0x25B044: [None, 0x0000],
            },
            check.DSP_PARAMETER_RUNTIME_OBJECT_EVENT_BOUNDED_ARGUMENTS,
        )
        self.assertEqual(
            {
                "records": 124,
                "unique_values": 32,
                "rom_catalogue_values": 8,
                "emitter_eligible_records": 31,
                "emitter_eligible_rom_catalogue_values": 0,
                "dispatch_bit_eligible_records": 0,
            },
            check.DSP_PARAMETER_FIXED_OBJECT_VALUE_CENSUS,
        )
        self.assertEqual(
            {
                0x22D686: {
                    "value": 0x0013,
                    "flags": 0x00,
                    "event": 0x0389,
                },
                0x22D6A0: {
                    "value": 0x0010,
                    "flags": 0x00,
                    "event": 0x0389,
                },
            },
            check.DSP_PARAMETER_EXPLICIT_OBJECT_EVENT_DESCRIPTORS,
        )
        self.assertEqual(
            {
                "registration_callsite": 0x278792,
                "stored_event": 0x0387,
                "value": 0x2B01D8,
                "flags": 0x40,
                "producer_callsite": 0x25B044,
                "constructor_event": 0x0389,
                "constructor_callsite": 0x27C17C,
            },
            check.DSP_PARAMETER_RUNTIME_OBJECT_INSTALLER,
        )
        self.assertEqual(
            {
                "address": 0x2B01D8,
                "records": 0x2B00E8,
                "record_count": 9,
                "events": [
                    0x00DC,
                    0x05E0,
                    0x05E0,
                    0x05E0,
                    0x0387,
                    0x05E0,
                    0x05E0,
                    0x0387,
                    0x01F4,
                ],
                "values": [
                    0,
                    0x47,
                    0x0D,
                    0x0C,
                    0x2B00DC,
                    0x6D,
                    0x0B,
                    0x2B043C,
                    0,
                ],
                "record_flags": [0, 8, 8, 8, 8, 8, 8, 8, 8],
            },
            check.DSP_PARAMETER_RUNTIME_OBJECT_CATALOGUE,
        )
        self.assertEqual(0x2BEAE8, check.DSP_PARAMETER_PPM_ROOT_POINTER_CELL)
        self.assertEqual(0x2C0000, check.DSP_PARAMETER_PPM_ROOT)
        self.assertEqual(
            [
                {
                    "address": 0x2C002C,
                    "length": 0x0234,
                    "tag": 0x4C504353,
                },
                {
                    "address": 0x2C0260,
                    "length": 0x01B4,
                    "tag": 0x47534D43,
                },
                {
                    "address": 0x2C0414,
                    "length": 0x28BC,
                    "tag": 0x464F4E54,
                },
                {
                    "address": 0x2C2CD0,
                    "length": 0x35998,
                    "tag": 0x54455854,
                },
            ],
            check.DSP_PARAMETER_PPM_TOP_LEVEL_NODES,
        )
        self.assertEqual(
            [0x01, 0x02, 0x0D, 0x0F, 0x18, 0x1A, 0x1B, 0x13],
            check.DSP_PARAMETER_PPM_DESCRIPTOR_VALUES,
        )
        self.assertEqual(
            [],
            check.DSP_PARAMETER_UNRESOLVED_RUNTIME_VALUE_CALLS,
        )
        self.assertEqual(
            {
                "calls": 1105,
                "resolved_sizes": 924,
                "runtime_sizes": 181,
                "exact_28_byte_calls": [
                    0x214958,
                    0x21522E,
                    0x240922,
                    0x256D4E,
                    0x256DD4,
                    0x27ACDE,
                    0x27CD80,
                    0x27E128,
                    0x28EDFA,
                ],
            },
            check.DSP_PARAMETER_ALLOCATOR_CENSUS,
        )
        self.assertEqual(
            {
                "allocation_callsite": 0x28EDFA,
                "allocation_size": 0x1C,
                "eeprom_read_callsite": 0x28EE0C,
                "eeprom_address": 0x40,
                "eeprom_length": 0x1C,
                "release_callsite": 0x28EF0A,
                "payload_event_offset": 0x12,
                "payload_event_value": "eeprom_data",
                "eeprom_field_writer": {
                    "request_handler": 0x238218,
                    "request_type": 0xCB,
                    "selector_message_offset": 9,
                    "payload_message_offset": 10,
                    "writer": 0x28ECEC,
                    "field_selector": 4,
                    "field_block_offsets": [0x11, 0x12, 0x13, 0x14],
                    "event_offsets": [0x12, 0x13],
                    "write_callsite": 0x28EDCC,
                },
                "payload_event_mutability": "runtime_request_writable",
            },
            check.DSP_PARAMETER_STALE_EVENT_REUSE_OWNER,
        )
        self.assertEqual(
            {
                "calls": 44,
                "resolved_addresses": 32,
                "runtime_addresses": 12,
                "event_byte_range": [0x52, 0x54],
                "resolved_overlapping_calls": [
                    {
                        "callsite": 0x28EDCC,
                        "address": 0x40,
                        "length": 0x1C,
                    },
                ],
                "field_writer_direct_calls": [0x23822A],
            },
            check.DSP_PARAMETER_EEPROM_WRITE_CENSUS,
        )
        self.assertEqual(0x2A5008, check.NSE3_COPY_TABLE_ADDRESS)
        self.assertEqual(
            [0x256E2E],
            check.DSP_PARAMETER_UNRESOLVED_RUNTIME_DESCRIPTORS,
        )
        self.assertEqual(
            {
                "callsite": 0x256E2E,
                "allocator": 0x260ABC,
                "allocation_size": 0x1C,
                "explicit_events": [0x13CE, 0x13CF],
                "unwritten_event_branch": 0x256E0C,
                "allocator_clears_payload": False,
                "value_field_source": "zero_extended_byte_loop_index",
                "value_field_can_be_rom_address": False,
            },
            check.DSP_PARAMETER_UNRESOLVED_RUNTIME_DESCRIPTOR_REASON,
        )
        self.assertEqual(
            29, len(check.DSP_PARAMETER_RUNTIME_DESCRIPTOR_EVENTS)
        )

    def test_dsp_bootstrap_result_rejects_generic_ready_one(self):
        physical = bytearray(b"\xff" * check.FLASH_SIZE)
        pc = 0x298E86
        physical[pc - check.FLASH_BASE : pc - check.FLASH_BASE + 2] = (
            bytes.fromhex("1248")
        )
        generic_ready = 1
        raw = ((generic_ready << 16) | (generic_ready >> 16)) & 0xFFFFFFFF
        struct.pack_into("<I", physical, 0x298ED0 - check.FLASH_BASE, raw)
        with self.assertRaisesRegex(ValueError, "expected 0xb06"):
            check.verify_thumb_literals(
                check.swap16(bytes(physical)), {pc: 0x0B06}
            )

    def test_second_bootstrap_word_has_no_direct_mcu_consumer(self):
        self.assertEqual(0x10B970, check.DSP_BOOTSTRAP_STATE_BASE)
        self.assertEqual(
            [
                0x28574A, 0x285842, 0x2859E4, 0x285A50, 0x285B1C,
                0x285D0C, 0x285D2C, 0x285D4C, 0x285D6E, 0x285E9E,
                0x285F3C, 0x297104, 0x297246,
            ],
            check.DSP_BOOTSTRAP_STATE_BASE_REFERENCES,
        )
        self.assertEqual(
            {
                0x297358: 0x20001,
                0x260254: 0x2000C,
                0x26025A: 0x100020,
                0x260260: 0x10C284,
            },
            check.DSP_BOOTSTRAP_POST_LITERALS,
        )
        self.assertEqual(
            0x286098, check.DSP_EXTERNAL_SOFTWARE_STRING
        )
        self.assertEqual(
            b" 25.3.531 \n17-Dec-97\nNSE-3Nx\n(c) NMP.\x00",
            check.DSP_EXTERNAL_SOFTWARE_BYTES,
        )
        self.assertEqual(
            {
                0x237BF2: 0x10BCF0,
                0x237BF6: 0x10BCFC,
                0x28EA16: 0x10BCF0,
                0x28EB02: 0x10BCE4,
            },
            check.DSP_INTERNAL_SOFTWARE_LITERALS,
        )
        self.assertEqual(0x2B74B0, check.NSE3_TASK_TABLE)
        self.assertEqual(12, check.NSE3_TASK_RECORD_SIZE)
        self.assertEqual(0x2B74C8, check.NSE3_TASK_2_ENTRY_POINTER)
        self.assertEqual(0x23A5CF, check.NSE3_TASK_2_ENTRY)
        self.assertEqual(
            ("movs", "r1, #2"),
            check.DSP_INTERNAL_SOFTWARE_OWNER_ANCHORS[0x2857E2],
        )
        self.assertEqual(
            ("bl", "#0x28580a"),
            check.DSP_INTERNAL_SOFTWARE_OWNER_ANCHORS[0x2A2216],
        )
        self.assertEqual(
            ("bl", "#0x2a1f52"),
            check.DSP_INTERNAL_SOFTWARE_OWNER_ANCHORS[0x2A2224],
        )


if __name__ == "__main__":
    unittest.main()
