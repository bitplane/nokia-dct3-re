#!/usr/bin/env python3

import unittest

from tools.flash_persistent_trace_check import check


FRONTIER = "external_service: response command=64 result=01 sequence=42\n"


class FlashPersistentTraceCheckTest(unittest.TestCase):
	def test_accepts_frontier_without_storage_access(self):
		check(FRONTIER)

	def test_rejects_incomplete_run(self):
		with self.assertRaisesRegex(ValueError, "frontier"):
			check("")

	def test_rejects_read(self):
		with self.assertRaisesRegex(ValueError, "read"):
			check(FRONTIER + "flash_persistent_read: pc=00300000\n")

	def test_rejects_write(self):
		with self.assertRaisesRegex(ValueError, "wrote"):
			check(FRONTIER + "flash_persistent_write: pc=00300000\n")


if __name__ == "__main__":
	unittest.main()
