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

	def test_big_endian_bl_and_exact_local_bytes(self):
		data = bytes.fromhex("f0 00 f8 00 20 01 e0 00")
		self.assertEqual(relocation_mask(data, "big"), bytearray((0, 0, 0, 0, 255, 255, 255, 255)))
		self.assertNotEqual(relocation_mask(data), relocation_mask(data, "big"))

	def test_invalid_byte_order(self):
		with self.assertRaises(ValueError):
			relocation_mask(b"", "swap16")


if __name__ == "__main__":
	unittest.main()
