import unittest

import capstone

from tools import mad2_static_census


class Mad2StaticCensusTests(unittest.TestCase):
	def test_swap16_literal_and_direct_halfword_access(self):
		# 200000: ldr r2,[pc,#4]; 200002: ldrh r3,[r2,#6]
		# The swap16 pool representation 0x00000002 rotates to 0x00020000.
		image = bytes.fromhex("014ad38800bf00bf02000000")
		accesses, coverage = mad2_static_census.analyze_image(image, max_seed_span=8)
		self.assertEqual(1, coverage["literal_seeds"])
		self.assertEqual([(0x200002, "read", 2, 6)],
			[(item["pc"], item["kind"], item["width"], item["offset"]) for item in accesses])

	def test_pointer_copy_and_offset_are_followed(self):
		md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
		md.detail = True
		# adds r1,r2,#4; ldrb r0,[r1,#8] => MAD2 offset 0x0c.
		instructions = list(md.disasm(bytes.fromhex("111d087a"), 0x200000))
		pointers = {"r2": mad2_static_census.MAD2_BASE}
		mad2_static_census.apply_pointer_update(instructions[0], pointers, b"", 0x200000)
		access = mad2_static_census.memory_access(instructions[1], pointers)
		self.assertEqual(0x0c, access["offset"])
		self.assertEqual("read", access["kind"])

	def test_call_invalidates_volatile_pointer(self):
		md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
		md.detail = True
		call = next(md.disasm(bytes.fromhex("00f000f8"), 0x200000))
		pointers = {"r0": mad2_static_census.MAD2_BASE, "r4": mad2_static_census.MAD2_BASE}
		for reg in ("r0", "r1", "r2", "r3"):
			pointers[reg] = None
		self.assertIsNone(pointers["r0"])
		self.assertEqual(mad2_static_census.MAD2_BASE, pointers["r4"])
		self.assertEqual("bl", call.mnemonic)

	def test_constant_register_index_is_opt_in(self):
		md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
		md.detail = True
		# movs r1,#0x40; ldrb r0,[r2,r1]
		load_constant, read = list(md.disasm(bytes.fromhex("4021505c"), 0x200000))
		pointers = {"r2": mad2_static_census.MAD2_BASE}
		mad2_static_census.apply_pointer_update(load_constant, pointers, b"", 0x200000, True)
		self.assertIsNone(mad2_static_census.memory_access(read, pointers))
		self.assertEqual(0x40, mad2_static_census.memory_access(
			read, pointers, constant_indexes=True)["offset"])

	def test_unknown_register_index_stays_unresolved(self):
		md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
		md.detail = True
		read = next(md.disasm(bytes.fromhex("505c"), 0x200000))
		self.assertIsNone(mad2_static_census.memory_access(read,
			{"r2": mad2_static_census.MAD2_BASE}, constant_indexes=True))

	def test_clobbered_index_and_outside_window_rejected(self):
		md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
		md.detail = True
		clobber, read = list(md.disasm(bytes.fromhex("0168505c"), 0x200000))
		pointers = {"r2": mad2_static_census.MAD2_BASE, "r1": 0x40}
		mad2_static_census.apply_pointer_update(clobber, pointers, b"", 0x200000, True)
		self.assertIsNone(mad2_static_census.memory_access(read, pointers, constant_indexes=True))
		pointers["r1"] = 0x100
		self.assertIsNone(mad2_static_census.memory_access(read, pointers, constant_indexes=True))

	def test_report_labels_candidate_mode(self):
		self.assertIn("linear-scan candidates", mad2_static_census.markdown([], True))

	def test_markdown_preserves_reviewed_contract_summary(self):
		report = mad2_static_census.summarize("fixture", "fixture.bin", [], {
			"literal_seeds": 0, "resolved_accesses": 0, "seed_terminations": {}})
		text = mad2_static_census.markdown([report])
		self.assertIn("Reviewed clock/reset anchors", text)
		self.assertIn("round((destination - current) / 8)", text)


if __name__ == "__main__":
	unittest.main()
