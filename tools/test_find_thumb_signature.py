#!/usr/bin/env python3
import unittest

from tools.find_thumb_signature import relocation_mask


class RelocationMaskTests(unittest.TestCase):
	def test_direct_thumb_bl_is_masked(self):
		data = bytes.fromhex("00 f0 00 f8 01 20")
		self.assertEqual(relocation_mask(data), bytearray((0, 0, 0, 0, 0xff, 0xff)))

	def test_local_branch_and_literals_remain_exact(self):
		data = bytes.fromhex("00 e0 12 34 78 56")
		self.assertEqual(relocation_mask(data), bytearray((0xff,) * len(data)))


if __name__ == "__main__":
	unittest.main()
