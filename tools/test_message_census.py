#!/usr/bin/env python3
import unittest
import sys
import tempfile
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


class RuntimeManifestTests(unittest.TestCase):
	def test_patterns_are_scoped_to_manifest_subsystem(self):
		profile = {"runtime_patterns": [
			{"kind": "contact", "subsystem": "contact_service", "regex": r"contact (?P<status>[0-9a-f]{4})"},
			{"kind": "gsm", "subsystem": "generic_service", "regex": r"gsm (?P<status>[0-9a-f]{4})"}
		]}
		with tempfile.TemporaryDirectory() as directory:
			path = Path(directory) / "error.log"
			path.write_text("contact 0064\ngsm 05e8\n")
			records = message_census.load_runtime_records(profile, "contact", {"contact_service"}, [path])
		self.assertEqual([record["kind"] for record in records], ["contact"])
		self.assertEqual(records[0]["manifest"], "contact")
		self.assertEqual(records[0]["subsystem"], "contact_service")

	def test_missing_manifest_does_not_claim_runtime_availability(self):
		manifests = [{"available": False, "subsystems": ["generic_service"]}]
		self.assertFalse(message_census.subsystem_runtime_available(manifests, "generic_service"))


if __name__ == "__main__":
	unittest.main()
