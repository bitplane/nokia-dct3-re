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


if __name__ == "__main__":
    unittest.main()
