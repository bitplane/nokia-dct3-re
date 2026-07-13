#!/usr/bin/env python3
import unittest
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import message_census


class ByteLaneTests(unittest.TestCase):
	def test_cpu_byte_crosses_swap16_lane(self):
		data = bytes((0x11, 0x22, 0x33, 0x44))
		self.assertEqual(message_census.cpu_byte(data, 0x1000, 0x1000), 0x22)
		self.assertEqual(message_census.cpu_byte(data, 0x1000, 0x1001), 0x11)

	def test_effective_pool_literal_rotates_halfwords(self):
		self.assertEqual(message_census.effective_u32(bytes((0x11, 0x22, 0x33, 0x44)), 0), 0x22114433)


if __name__ == "__main__":
	unittest.main()
