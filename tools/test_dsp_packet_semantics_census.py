import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

from tools.dsp_packet_semantics_census import classify, parse


class DspPacketSemanticsCensusTest(unittest.TestCase):
	def test_classification(self):
		self.assertEqual(("service_control_request", "request-derived type-0x74 completion"),
				classify("tx", 0x70, bytes.fromhex("0d00")))
		self.assertEqual(("service_control_completion", "derived from type-0x70/0d00"),
				classify("rx", 0x74, bytes.fromhex("0d00")))
		self.assertEqual(("service_control_followup", "one-way publication after type-0x74 completion"),
				classify("tx", 0x70, bytes.fromhex("0a09")))
		self.assertEqual(("bootstrap_platform_word", "one-way DSP bootstrap publication"),
				classify("tx", 0x70, bytes.fromhex("13042386fef6")))
		self.assertEqual(("bootstrap_table", "one-way DSP bootstrap publication"),
				classify("tx", 0x70, bytes.fromhex("140c" + "ff" * 12)))
		self.assertEqual(("segmented_dsp_memory_upload", "one-way command-0x22 DSP memory image"),
				classify("tx", 0x51, bytes.fromhex("2206" + "00" * 78)))
		self.assertEqual(("search_list", "asynchronous GSM channel search command"),
				classify("tx", 0x1A, bytes(68)))
		self.assertEqual(("cipher_control", "one-way DSP cipher-control publication"),
				classify("tx", 0x14, bytes.fromhex("00f4" + "ff" * 8 + "0000")))
		self.assertEqual(("indexed_64_byte_block_upload", "one-way DSP configuration publication"),
				classify("tx", 0x0D, bytes(66)))
		self.assertEqual(("selector_lookup_table_upload", "one-way DSP configuration publication"),
				classify("tx", 0x3C, bytes(156)))
		self.assertEqual(("external_discovery_control", "one-way discovery-side control publication"),
				classify("tx", 0x05, bytes.fromhex("1e1400f400010300")))
		self.assertEqual(("external_discovery_close", "one-way acknowledgement of peer state-4 completion"),
				classify("tx", 0x05, bytes.fromhex("1e0200d0000305014100")))
		self.assertEqual(("service_empty_report", "one-way packet; completion uses DSP shared control"),
				classify("tx", 0x05, bytes.fromhex("1e020000000a01016206008d010001c3")))

	def test_parse_notification(self):
		with TemporaryDirectory() as directory:
			path = Path(directory) / "error.log"
			path.write_text(
				"dspif_transport: RX enqueue type=74 payload=2 producer=08e data=0d00 t=0.5\n"
				"dspif_transport: FIQ0 notify producer=08e consumer=08c t=0.5\n"
			)
			items = parse("v600", path)
		self.assertEqual(1, len(items))
		self.assertTrue(items[0]["notified"])


if __name__ == "__main__":
	unittest.main()
