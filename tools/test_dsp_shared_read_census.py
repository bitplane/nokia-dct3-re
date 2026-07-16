import unittest

from tools.dsp_shared_read_census import classify, parse


class DspSharedReadCensusTest(unittest.TestCase):
	def test_classification(self):
		self.assertEqual(("bootstrap_ready", "DSP peer"), classify(0x004, 1))
		self.assertEqual(("shared_ram_self_test", "MCU echo"), classify(0x004, 0xFF))
		self.assertEqual(("tx_consumer", "DSP peer"), classify(0x0A6))
		self.assertEqual(("shared_control_request", "MCU request / DSP completion"), classify(0x0DC))
		self.assertEqual(("rx_consumer", "MCU"), classify(0x1CA))
		self.assertEqual(("unclassified", "unknown"), classify(0x700))

	def test_parse(self):
		from pathlib import Path
		from tempfile import TemporaryDirectory
		with TemporaryDirectory() as directory:
			path = Path(directory) / "error.log"
			path.write_text("dsp_shared_read: off=0e0 data=0000 pc=00290000 t=0.5\n")
			items = parse("v600", path)
		self.assertEqual(1, len(items))
		self.assertEqual("shared_control_busy", items[0]["role"])


if __name__ == "__main__":
	unittest.main()
