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

def si_parse_line(message_type: int, time: float) -> str:
	return (
		f"nhm5_bcch_parse: channel=50 block=0011727c "
		f"data=49 06 {message_type:02x} 00 00 task=b2 t={time}\n"
	)

def si_complete_line(time: float) -> str:
	return (
		"nhm5_si_result: message=19 changed=01 result=00000000 "
		f"channel=50 flags=0f/00/33 status=0002 ready=00 gate=00000004 "
		f"task=b2 t={time}\n"
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
			sch = "4012000000000058" + "00" * 26
			configure = "04120200000000505000005800000000000a98fa"
			idle_configure = "0412020900000010600000581000000000297000"
			si1 = "50120000000000580000" + "550619" + "00" * 5 + "80" + "00" * 15
			pch = "60120000000000580000" + "1506210001f0" + "2b" * 18
			path.write_text(
				line(0x20, type20, 0.1)
				+ line(0x21, type21 + "00000319", 0.2)
				+ line(0x22, type22, 0.3)
				+ line(0x20, type20, 0.4)
				+ line(0x21, type21 + "00000140", 0.5)
				+ line(0x22, type22, 0.6)
				+ line(0x56, "0058" + "ff" * 158, 2.0)
				+ rx_line(0x80, sch, 2.1)
				+ line(0x02, configure, 2.2)
				+ rx_line(0x8F, "00" * 8, 2.3)
				+ rx_line(0x89, "00" * 8, 2.4)
				+ rx_line(0x84, "00" * 8, 2.5)
				+ rx_line(0x80, si1, 2.6)
				+ "".join(si_parse_line(message_type, 2.7 + index / 10)
					for index, message_type in enumerate((0x19, 0x1A, 0x1B, 0x1C)))
				+ si_complete_line(3.1)
				+ line(0x02, idle_configure, 3.2)
				+ rx_line(0x80, pch, 3.3)
				+ rx_line(0x80, si1, 3.4)
			)
			check_nhm5_search(path)

	def test_nhm5_search_rejects_failure_on_success_path(self):
		with TemporaryDirectory() as directory:
			path = Path(directory) / "error.log"
			type20 = "00" * 68
			type21 = "11" * 28
			type22 = "22" * 32
			sch = "4012000000000058" + "00" * 26
			configure = "04120200000000505000005800000000000a98fa"
			si1 = "50120000000000580000" + "550619" + "00" * 5 + "80" + "00" * 15
			path.write_text(
				line(0x20, type20, 0.1)
				+ line(0x21, type21 + "00000319", 0.2)
				+ line(0x22, type22, 0.3)
				+ line(0x20, type20, 0.4)
				+ line(0x21, type21 + "00000140", 0.5)
				+ line(0x22, type22, 0.6)
				+ line(0x56, "0058" + "ff" * 158, 2.0)
				+ rx_line(0x80, sch, 2.1)
				+ line(0x02, configure, 2.2)
				+ rx_line(0x8F, "00" * 8, 2.3)
				+ rx_line(0x89, "00" * 8, 2.4)
				+ rx_line(0x84, "00" * 8, 2.5)
				+ rx_line(0x80, si1, 2.6)
				+ "".join(si_parse_line(message_type, 2.7 + index / 10)
					for index, message_type in enumerate((0x19, 0x1A, 0x1B, 0x1C)))
				+ si_complete_line(3.1)
				+ rx_line(0x8A, "0058" + "00" * 6, 2.7)
			)
			with self.assertRaises(SystemExit):
				check_nhm5_search(path)

	def test_swap16(self):
		self.assertEqual(bytes.fromhex("01000302"), swap16(bytes.fromhex("00010203")))


if __name__ == "__main__":
	unittest.main()
