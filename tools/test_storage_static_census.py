#!/usr/bin/env python3
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import capstone
import mad2_static_census


class StorageStaticCensusTests(unittest.TestCase):
	def test_generic_window_tracks_direct_access(self):
		md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
		md.detail = True
		insns = list(md.disasm(bytes.fromhex("02480178"), 0x200000))
		access = mad2_static_census.memory_access(
			insns[1], {"r0": 0x00A00000}, 0x00A00000, 0x4000)
		self.assertEqual(access["address"], 0x00A00000)
		self.assertEqual(access["kind"], "read")

	def test_generic_window_rejects_neighbor(self):
		md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
		md.detail = True
		insn = list(md.disasm(bytes.fromhex("0178"), 0x200000))[0]
		self.assertIsNone(mad2_static_census.memory_access(
			insn, {"r0": 0x00A04000}, 0x00A00000, 0x4000))


if __name__ == "__main__":
	unittest.main()
