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

	def test_callback_transition_decodes_swap16_byte_lanes(self):
		# CPU bytes are 5d,03,04,00 and the packed halfword is 0x551c.
		data = bytes((0x03, 0x5d, 0x00, 0x04, 0x1c, 0x55, 0x00, 0x00))
		profile = {"callback_transition_table": {
			"address": "0x2000", "entries": 1, "entry_size": 8}}
		self.assertEqual(message_census.extract_callback_transitions(profile, data, 0x2000), [{
			"index": 0, "address": 0x2000, "selector": 0x5d,
			"required_state": 3, "control": 4, "new_state": 4,
			"inverted_match": False, "packed_event": 0x551c,
			"event": 0x151c, "argument_count": 1,
			"provenance": "extracted_static"}])

	def test_status_descriptor_extent_uses_halfword_and_byte_lanes(self):
		# Descriptor: status 0x0367, transition start 0x00eb, count 6.
		data = bytes((0x67, 0x03, 0xeb, 0x00, 0x00, 0x06, 0x00, 0x00))
		profile = {"status_descriptor_table": {
			"address": "0x2000", "entries": 1, "entry_size": 8}}
		self.assertEqual(message_census.status_descriptor_transition_extent(
			profile, data, 0x2000), 0xf1)

	def test_computed_r0_construction_tracks_straight_line_shift(self):
		# movs r0,#0xef; lsls r0,r0,#3 -> 0x0778
		data = bytes((0xef, 0x20, 0xc0, 0x00))
		instructions = message_census.decode_image(data, 0x2000)
		result = message_census.computed_r0_constructions(instructions, data, 0x2000, 0x0778)
		self.assertEqual([item["address"] for item in result], [0x2002])

	def test_computed_r0_construction_does_not_cross_branch(self):
		# movs r0,#0xef; b +0; lsls r0,r0,#3
		data = bytes((0xef, 0x20, 0x00, 0xe0, 0xc0, 0x00))
		instructions = message_census.decode_image(data, 0x2000)
		self.assertEqual(message_census.computed_r0_constructions(
			instructions, data, 0x2000, 0x0778), [])

	def test_catalogue_predecessor_uses_effective_word_layout(self):
		# Effective entries 0x089a and 0x08b0 in swap16 storage.
		data = bytes((0x00, 0x00, 0x9a, 0x08, 0x00, 0x00, 0xb0, 0x08))
		contract = {"address": 0x2000, "entries": 2, "entry_size": 4}
		result = message_census.catalogue_predecessors(contract, data, 0x2000, 0x08b0)
		self.assertEqual(result[-1], {
			"status_index": 1, "packed_inputs": [0x2001, 0x6001], "entry": 0x2004,
			"sequence_offset": 0,
			"producer_evidence": {"literal_loads": [],
				"computed_r0_constructions": [], "direct_calls": []}})

	def test_catalogue_predecessor_decodes_sequence_and_skips_arguments(self):
		# Entry 0: packed argc-1 event 0x0123, argument 0x08b0, then event 0x08b0.
		words = (0x4123, 0x08b0, 0x08b0, 0x00dc)
		data = b"".join(((word >> 16).to_bytes(2, "little") +
			(word & 0xffff).to_bytes(2, "little")) for word in words)
		contract = {"address": 0x2000, "entries": 1, "entry_size": 4}
		result = message_census.catalogue_predecessors(contract, data, 0x2000, 0x08b0)
		self.assertEqual(result[0]["sequence_offset"], 1)

	def test_status_inventory_accepts_decoded_packed_event(self):
		calls = [{"callsite": 0x2000, "api": "task5_render_post", "arguments": {
			"packed_event": 0x53f8, "event": 0x13f8, "argument_count": 1,
			"argument_words": [None], "descriptor": None}}]
		result = message_census.status_inventory([], calls, [], b"", 0x200000, 0x13f8)
		self.assertEqual(result["call_arguments"], [{
			"callsite": 0x2000, "api": "task5_render_post", "argument": "event"}])

	def test_rom_encoding_inventory_separates_loaded_mcu_and_payload_words(self):
		# ldr r0,[pc,#0] loads effective 0x53f8 at 0x2004; a second copy is payload.
		data = bytes((0x00, 0x48, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x53,
			0x00, 0x00, 0xf8, 0x53))
		instructions = message_census.decode_image(data, 0x2000)
		result = message_census.rom_encoding_inventory(
			instructions, data, 0x2000, 0x13f8, mcu_limit=0x2008)
		self.assertEqual(result["packed_words"], [
			{"address": 0x2004, "value": 0x53f8, "region": "mcu", "literal_loads": [0x2000]},
			{"address": 0x2008, "value": 0x53f8, "region": "ppm_or_payload", "literal_loads": []}])


class ContactServiceTests(unittest.TestCase):
	def test_constructor_role_is_separate_from_numeric_command(self):
		profile = {"external_service_commands": [{
			"command": "0x70", "name": "enable", "consumer": "0x2000",
			"expected_constructor": {"callsite": "0x2100", "payload_length": 1},
			"constructor_role": "ack", "producer_class": "unresolved",
			"confidence": "high", "evidence": "fixture"
		}]}
		calls = [{"callsite": 0x2100, "api": "service_message_alloc",
			"arguments": {"command": 0x70, "payload_length": 1}}]
		runtime = [{"kind": "contact_receive", "fields": {"command": 0x70},
			"file": "deep.log", "count": 3}]
		result = message_census.external_service_inventory(profile, calls, runtime)
		self.assertTrue(result["all_constructor_anchors_valid"])
		self.assertEqual(result["commands"][0]["constructors"][0]["callsite"], 0x2100)
		self.assertEqual(result["commands"][0]["runtime"]["contact_receive"]["occurrences"], 3)
		self.assertEqual(result["commands"][0]["producer_class"], "unresolved")


class RuntimeManifestTests(unittest.TestCase):
	def test_patterns_are_scoped_to_manifest_subsystem(self):
		profile = {"runtime_patterns": [
			{"kind": "contact", "subsystem": "external_service", "regex": r"contact (?P<status>[0-9a-f]{4})"},
			{"kind": "gsm", "subsystem": "generic_service", "regex": r"gsm (?P<status>[0-9a-f]{4})"}
		]}
		with tempfile.TemporaryDirectory() as directory:
			path = Path(directory) / "error.log"
			path.write_text("contact 0064\ngsm 05e8\n")
			records = message_census.load_runtime_records(profile, "contact", {"external_service"}, [path])
		self.assertEqual([record["kind"] for record in records], ["contact"])
		self.assertEqual(records[0]["manifest"], "contact")
		self.assertEqual(records[0]["subsystem"], "external_service")

	def test_missing_manifest_does_not_claim_runtime_availability(self):
		manifests = [{"available": False, "subsystems": ["generic_service"]}]
		self.assertFalse(message_census.subsystem_runtime_available(manifests, "generic_service"))


class DescriptorClassificationTests(unittest.TestCase):
	def test_fixed_ram_descriptor_is_not_a_service5_candidate(self):
		call = {"callsite": 0x2002, "arguments": {"descriptor": 0x110000, "service": 0x0a}}
		self.assertEqual(message_census.descriptor_storage(call, {}, b"", 0x200000), "fixed_ram")
		self.assertEqual(message_census.service5_candidacy(call), "excluded_other_service")

	def test_stack_descriptor_with_dynamic_service_remains_explicit(self):
		md = message_census.capstone.Cs(message_census.capstone.CS_ARCH_ARM,
				message_census.capstone.CS_MODE_THUMB)
		md.detail = True
		insn = next(md.disasm(bytes((0x6a, 0x46)), 0x2000))
		call = {"callsite": 0x2002, "arguments": {"descriptor": None, "service": None}}
		self.assertEqual(message_census.descriptor_storage(call, {0x2000: insn}, b"", 0x200000), "stack")
		self.assertEqual(message_census.service5_candidacy(call), "dynamic_service_unresolved")


class ObjectLifecycleTests(unittest.TestCase):
	def test_only_object_bearing_05e0_constructors_are_inventoried(self):
		calls = [
			{"callsite": 0x2000, "api": "generic_event_generate", "arguments": {
				"packed_event": 0x45e0, "event": 0x05e0, "argument_count": 1, "argument_words": [7]}},
			{"callsite": 0x2002, "api": "generic_event_generate", "arguments": {
				"packed_event": 0x85e0, "event": 0x05e0, "argument_count": 2, "argument_words": [7, 0x110000]}},
			{"callsite": 0x2004, "api": "generic_event_generate", "arguments": {
				"packed_event": 0xc5e0, "event": 0x05e0, "argument_count": 3, "argument_words": [8, 0x110100, 3]}}
		]
		assessments = [
			{"callsite": "0x2002", "classification": "dormant", "role": "fixture"},
			{"callsite": "0x2004", "classification": "unresolved", "role": "fixture"}
		]
		result = message_census.object_lifecycle_inventory(calls, assessments)
		self.assertEqual([item["callsite"] for item in result["constructors"]], [0x2002, 0x2004])
		self.assertEqual(result["constructors"][1]["extra_arguments"], [3])
		self.assertTrue(result["coverage_complete"])

	def test_assessment_coverage_reports_missing_and_stale_callsites(self):
		calls = [{"callsite": 0x2002, "api": "generic_event_generate", "arguments": {
			"packed_event": 0x85e0, "event": 0x05e0, "argument_count": 2, "argument_words": [7, 1]}}]
		result = message_census.object_lifecycle_inventory(calls,
			[{"callsite": "0x2004", "classification": "stale", "role": "fixture"}])
		self.assertEqual(result["missing_assessments"], [0x2002])
		self.assertEqual(result["stale_assessments"], [0x2004])
		self.assertFalse(result["coverage_complete"])

	def test_dynamic_inventory_bounds_object_bearing_candidates(self):
		calls = [
			{"callsite": 0x3000, "api": "generic_event_generate", "arguments": {"packed_event": None}},
			{"callsite": 0x3002, "api": "generic_event_generate", "arguments": {"packed_event": None}},
			{"callsite": 0x3004, "api": "task_post", "arguments": {"packed_event": None}}
		]
		assessments = [
			{"callsite": "0x3000", "argument_count": 1, "classification": "scalar", "role": "fixture"},
			{"callsite": "0x3002", "argument_count": 2, "possible_events": ["0x0594", "0x0c2d"],
				"classification": "bounded", "role": "fixture"}
		]
		result = message_census.dynamic_packed_event_inventory(calls, assessments)
		self.assertEqual(result["unresolved_calls"], 2)
		self.assertEqual(result["assessed_calls"], 2)
		self.assertEqual([item["callsite"] for item in result["object_bearing_candidates"]], ["0x3002"])
		self.assertTrue(result["coverage_complete"])
		self.assertFalse(result["can_publish_05e0"])

	def test_dynamic_inventory_rejects_unreviewed_or_05e0_candidate(self):
		calls = [
			{"callsite": 0x3000, "api": "generic_event_generate", "arguments": {"packed_event": None}},
			{"callsite": 0x3002, "api": "generic_event_generate", "arguments": {"packed_event": None}}
		]
		assessments = [
			{"callsite": "0x3000", "argument_count": 2, "possible_events": ["0x05e0"],
				"classification": "candidate", "role": "fixture"},
			{"callsite": "0x3004", "argument_count": 0, "classification": "stale", "role": "fixture"}
		]
		result = message_census.dynamic_packed_event_inventory(calls, assessments)
		self.assertEqual(result["missing_assessments"], [0x3002])
		self.assertEqual(result["stale_assessments"], [0x3004])
		self.assertFalse(result["coverage_complete"])
		self.assertTrue(result["can_publish_05e0"])


if __name__ == "__main__":
	unittest.main()
