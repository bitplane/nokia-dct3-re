import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

from tools.dsp_radio_profile_trace_check import (
	check_nhm5_search,
	check_nhm5_startup,
	swap16,
)


def line(packet_type: int, data: str, time: float) -> str:
	return (
		f"dspif_transport: TX consume type={packet_type:02x} payload={len(data) // 2} "
		f"producer=000 consumer=000 data={data} t={time}\n"
	)

def rx_line(packet_type: int, data: str, time: float) -> str:
	return (
		f"dspif_transport: RX enqueue type={packet_type:02x} payload={len(data) // 2} "
		f"producer=000 data={data} t={time}\n"
	)


class DspRadioProfileTraceCheckTest(unittest.TestCase):
	def test_nhm5_boundary(self):
		with TemporaryDirectory() as directory:
			path = Path(directory) / "error.log"
			type20 = "00" * 68
			type21 = "11" * 28
			type22 = "22" * 32
			path.write_text(
				line(0x20, type20, 0.1)
				+ line(0x21, type21 + "00000319", 0.2)
				+ line(0x22, type22, 0.3)
				+ line(0x20, type20, 0.4)
				+ line(0x21, type21 + "00000140", 0.5)
				+ line(0x22, type22, 0.6)
				+ line(0x56, "0058" + "ff" * 158, 2.0)
			)
			check_nhm5_startup(path)

	def test_rejects_nse8_search_list(self):
		with TemporaryDirectory() as directory:
			path = Path(directory) / "error.log"
			type20 = "00" * 68
			type21 = "11" * 28
			type22 = "22" * 32
			path.write_text(
				line(0x20, type20, 0.1)
				+ line(0x21, type21 + "00000319", 0.2)
				+ line(0x22, type22, 0.3)
				+ line(0x20, type20, 0.4)
				+ line(0x21, type21 + "00000140", 0.5)
				+ line(0x22, type22, 0.6)
				+ line(0x56, "0058" + "ff" * 158, 2.0)
				+ line(0x1A, "00" * 68, 2.1)
			)
			with self.assertRaises(SystemExit):
				check_nhm5_startup(path)

	def test_nhm5_search_frontier(self):
		with TemporaryDirectory() as directory:
			path = Path(directory) / "error.log"
			type20 = "00" * 68
			type21 = "11" * 28
			type22 = "22" * 32
			results = "0010005800c4" + "ff" * 160
			path.write_text(
				line(0x20, type20, 0.1)
				+ line(0x21, type21 + "00000319", 0.2)
				+ line(0x22, type22, 0.3)
				+ line(0x20, type20, 0.4)
				+ line(0x21, type21 + "00000140", 0.5)
				+ line(0x22, type22, 0.6)
				+ line(0x56, "0058" + "ff" * 158, 2.0)
				+ rx_line(0x8B, results, 2.1)
				+ line(0x55, "03050000", 2.2)
			)
			check_nhm5_search(path)

	def test_nhm5_search_rejects_unproved_power_sweep_result(self):
		with TemporaryDirectory() as directory:
			path = Path(directory) / "error.log"
			type20 = "00" * 68
			type21 = "11" * 28
			type22 = "22" * 32
			results = "0010005800c4" + "ff" * 160
			path.write_text(
				line(0x20, type20, 0.1)
				+ line(0x21, type21 + "00000319", 0.2)
				+ line(0x22, type22, 0.3)
				+ line(0x20, type20, 0.4)
				+ line(0x21, type21 + "00000140", 0.5)
				+ line(0x22, type22, 0.6)
				+ line(0x56, "0058" + "ff" * 158, 2.0)
				+ rx_line(0x8B, results, 2.1)
				+ line(0x55, "03050000", 2.2)
				+ rx_line(0x8A, "0058" + "00" * 6, 2.3)
			)
			with self.assertRaises(SystemExit):
				check_nhm5_search(path)

	def test_swap16(self):
		self.assertEqual(bytes.fromhex("01000302"), swap16(bytes.fromhex("00010203")))


if __name__ == "__main__":
	unittest.main()
