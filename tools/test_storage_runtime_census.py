#!/usr/bin/env python3
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import storage_runtime_census


class StorageRuntimeCensusTests(unittest.TestCase):
	def test_activity_requires_transaction_scale_traffic(self):
		with tempfile.TemporaryDirectory() as directory:
			path = Path(directory) / "summary.txt"
			path.write_text("eeprom_starts=239\neeprom_signal_writes=63956\n")
			report = storage_runtime_census.analyze("3210-v6.00", path)
		self.assertTrue(report["serial_eeprom_active"])

	def test_single_start_shaped_edge_is_not_activity(self):
		with tempfile.TemporaryDirectory() as directory:
			path = Path(directory) / "summary.txt"
			path.write_text("eeprom_starts=1\neeprom_signal_writes=14\n")
			report = storage_runtime_census.analyze("3310-v6.39", path)
		self.assertFalse(report["serial_eeprom_active"])


if __name__ == "__main__":
	unittest.main()
