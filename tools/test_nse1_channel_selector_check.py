import unittest
from pathlib import Path

from tools.nse1_channel_selector_check import check_image, expected_selector


class ChannelSelectorTests(unittest.TestCase):
    def test_explicit_speech_branch_and_unmatched_residual(self):
        for second in range(256):
            self.assertEqual(expected_selector(0x21, second), 0xc3)
            self.assertEqual(expected_selector(0xe4, second), 0xc3)
        self.assertEqual(expected_selector(2, 0), 0xffffffe1)

    def test_alternate_channel_modes(self):
        self.assertEqual(expected_selector(1, 0x20), 0xc0)
        self.assertEqual(expected_selector(1, 0x21), 0xc0)
        self.assertEqual(expected_selector(1, 0x22), 0xc1)
        self.assertEqual(expected_selector(19, 0x20), 0xd2)
        self.assertEqual(expected_selector(19, 0), 0xd3)

    def test_reject_unknown_rom(self):
        with self.assertRaisesRegex(ValueError, "identified unswapped"):
            check_image(bytes(0x100000))

    def test_acquired_rom_byte_domain(self):
        rom = Path(__file__).resolve().parents[1] / "roms/noki5110/5110f530.fls"
        if not rom.exists():
            self.skipTest("acquired NSE-1 ROM is not distributed with sources")
        result = check_image(rom.read_bytes())
        self.assertEqual(result["cases"], 65536)
        self.assertEqual(result["ring_copy_start_positions"], 82)
        self.assertEqual(result["request_routes"], 4)
        self.assertEqual(result["request_field_cases"], 1024)
        self.assertEqual(result["mode_writer_cases"], 256)
        self.assertEqual(result["mode_parser_cases"], 768)
        self.assertEqual(result["optional_mode_parser_cases"], 512)
        self.assertEqual(result["channel_description_cases"], 256)
        self.assertEqual(result["channel_parameter_cases"], 65536)
        self.assertEqual(result["rr_receive_route_cases"], 512)
        self.assertEqual(result["rr_payload_allocation_cases"], 1536)
        self.assertEqual(result["rr_fixed_block_cases"], 1024)
        self.assertEqual(result["rr_unsegmented_block_cases"], 256)
        self.assertEqual(result["rr_segment_append_cases"], 11264)
        self.assertEqual(result["rr_record_selection_cases"], 4096)
        self.assertEqual(result["incoming_block_constructor_cases"], 768)
        self.assertEqual(result["type80_dispatch_cases"], 2)
        self.assertEqual(result["receive_ring_copy_cases"], 1300)
        self.assertEqual(result["receive_ring_header_cases"], 65636)
        self.assertEqual(result["c3_first_arguments"], [0x21, 0xe4])


if __name__ == "__main__":
    unittest.main()
