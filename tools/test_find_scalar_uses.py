#!/usr/bin/env python3
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import capstone
import find_scalar_uses


class FindScalarUsesTests(unittest.TestCase):
	def test_literal_value_undoes_swap16_storage(self):
		decoder = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
		instruction = next(decoder.disasm(bytes.fromhex("0148"), 0x200000))
		image = bytes.fromhex("01480000000000001000cf7b")
		self.assertEqual(
			find_scalar_uses.literal_value(image, instruction),
			(0x200008, 0x00107bcf))


if __name__ == "__main__":
	unittest.main()
