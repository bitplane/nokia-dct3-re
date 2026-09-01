import unittest

from tools.board_io_static_census import region_for_offset


class BoardIoStaticCensusTest(unittest.TestCase):
	def test_sparse_region_boundaries(self):
		cases = {
			0x15: "PUP", 0x24: "PUP",
			0x28: "KBGPIO", 0x6b: "KBGPIO", 0xab: "KBGPIO",
			0x30: "UIF", 0x73: "UIF", 0xb2: "UIF", 0xf3: "UIF",
			0x6f: "SELECT", 0xad: "SELECT", 0xef: "SELECT",
			0x14: None, 0x25: None, 0x6d: None, 0xac: None,
		}
		for offset, expected in cases.items():
			with self.subTest(offset=offset):
				self.assertEqual(region_for_offset(offset), expected)


if __name__ == "__main__":
	unittest.main()
