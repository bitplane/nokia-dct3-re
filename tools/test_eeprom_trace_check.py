#!/usr/bin/env python3
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import eeprom_trace_check


class EepromTraceCheckTests(unittest.TestCase):
	LINE = ("eeprom_fixture: mode=write initial_ack=1 busy_ack=0 ready_ack=1 "
		"reads_ok=1 data_ok=1 data=a1,b2,c3,d4 t=2.506000\n")

	def test_accepts_write_cycle_contract(self):
		self.assertEqual(eeprom_trace_check.check(self.LINE, "write"), [])

	def test_rejects_immediate_busy_ack(self):
		errors = eeprom_trace_check.check(
			self.LINE.replace("busy_ack=0", "busy_ack=1"), "write")
		self.assertIn("device acknowledged during its self-timed write cycle", errors)

	def test_accepts_persisted_read(self):
		self.assertEqual(eeprom_trace_check.check(
			self.LINE.replace("mode=write", "mode=read"), "read"), [])


if __name__ == "__main__":
	unittest.main()
