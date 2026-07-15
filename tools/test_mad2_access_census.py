import unittest

from tools import mad2_access_census


class Mad2AccessCensusTests(unittest.TestCase):
	def test_parses_read_and_write(self):
		text = """[:] mad2_ledger: R off=00 data=40 pc=002afc1e t=0.007183 [CTSI] ASIC\n
[:] mad2_ledger: W off=0f data=f9 old=3f pc=002aa93a t=0.159242 [CTSI] Timer\n"""
		summary = mad2_access_census.summarize(mad2_access_census.parse(text))
		self.assertEqual(summary["records"], 2)
		self.assertEqual(summary["read_offsets"], [["IO", 0x00]])
		self.assertEqual(summary["write_offsets"], [["IO", 0x0f]])
		self.assertEqual(summary["accesses"][1]["old"], 0x3f)

	def test_detects_duplicate_first_access(self):
		line = "mad2_ledger: R off=31 data=00 pc=002a6668 t=0.1 <Unknown>\n"
		summary = mad2_access_census.summarize(mad2_access_census.parse(line + line))
		self.assertEqual(summary["duplicate_first_accesses"], 1)
		self.assertEqual(summary["unknown_offsets"], [("IO", 0x31)])

	def test_separates_interface_windows(self):
		text = """mad2_ledger: W bus=DSPIF off=00 data=02 old=00 pc=002001a4 t=0.1 <Unknown>\n
mad2_ledger: W bus=MCUIF off=00 data=02 old=00 pc=002001ac t=0.1 <Unknown>\n"""
		summary = mad2_access_census.summarize(mad2_access_census.parse(text))
		self.assertEqual(summary["duplicate_first_accesses"], 0)


if __name__ == "__main__":
	unittest.main()
