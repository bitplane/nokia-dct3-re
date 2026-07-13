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


class ContactServiceTests(unittest.TestCase):
	def test_constructor_role_is_separate_from_numeric_command(self):
		profile = {"contact_service_commands": [{
			"command": "0x70", "name": "enable", "consumer": "0x2000",
			"expected_constructor": {"callsite": "0x2100", "payload_length": 1},
			"constructor_role": "ack", "producer_class": "unresolved",
			"confidence": "high", "evidence": "fixture"
		}]}
		calls = [{"callsite": 0x2100, "api": "contact_message_alloc",
			"arguments": {"command": 0x70, "payload_length": 1}}]
		runtime = [{"kind": "contact_receive", "fields": {"command": 0x70},
			"file": "deep.log", "count": 3}]
		result = message_census.contact_service_inventory(profile, calls, runtime)
		self.assertTrue(result["all_constructor_anchors_valid"])
		self.assertEqual(result["commands"][0]["constructors"][0]["callsite"], 0x2100)
		self.assertEqual(result["commands"][0]["runtime"]["contact_receive"]["occurrences"], 3)
		self.assertEqual(result["commands"][0]["producer_class"], "unresolved")


if __name__ == "__main__":
	unittest.main()
