import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

from tools.dsp_radio_profile_trace_check import check_nhm5_startup


def line(packet_type: int, data: str, time: float) -> str:
	return (
		f"dspif_transport: TX consume type={packet_type:02x} payload={len(data) // 2} "
		f"producer=000 consumer=000 data={data} t={time}\n"
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


if __name__ == "__main__":
	unittest.main()
