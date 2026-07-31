#!/usr/bin/env python3
"""Tests for the acceptance-gate parity audit."""

import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import gate_matrix
import gate_parity_audit


SAMPLE = "\n".join([
    "verify-radio-thing:",
    "\t@$(MAKE) --no-print-directory run PHONE=noki3210 SECONDS=20",
    "\tcp $(MAME_DIR)/error.log $(RUN_DIR)/error.log",
    "\t$(PYTHON) tools/thing_check.py $(RUN_DIR)/error.log --profile nse8 --strict",
    "",
    "verify-3310-radio-thing: normalize-3310 build",
    "\t@$(MAKE) --no-print-directory run PHONE=noki3310 SECONDS=30",
    "\tcp $(MAME_DIR)/error.log $(RUN_DIR)/error.log",
    "\t$(PYTHON) tools/thing_check.py $(RUN_DIR)/error.log --profile nhm5",
    "\t$(PYTHON) tools/extra_check.py $(RUN_DIR)/error.log",
    "",
    "verify-3330-radio-only-here:",
    "\t$(PYTHON) tools/thing_check.py $(RUN_DIR)/error.log",
    "",
])


class FamilyTest(unittest.TestCase):
    def test_unprefixed_gate_belongs_to_the_default_product(self):
        self.assertEqual(gate_parity_audit.family_of("verify-radio-paging"),
                         ("3210", "radio-paging"))

    def test_prefixed_gate_splits_product_and_family(self):
        self.assertEqual(gate_parity_audit.family_of("verify-3410-radio-paging"),
                         ("3410", "radio-paging"))

    def test_native_unit_test_gates_belong_to_no_product(self):
        self.assertIsNone(gate_parity_audit.family_of("verify-gsm-a5"))
        self.assertIsNone(gate_parity_audit.family_of("verify-gsm-fr-codec"))

    def test_bringup_gates_are_product_gates_not_neutral(self):
        """6110 is a product with one family, not a product-neutral gate."""
        self.assertEqual(gate_parity_audit.family_of("verify-6110-static"),
                         ("6110", "static"))


class AuditTest(unittest.TestCase):
    def setUp(self):
        matrix = gate_matrix.extract(SAMPLE)
        self.result = gate_parity_audit.audit(matrix)
        self.kinds = {f["kind"] for f in self.result["findings"]}

    def test_single_product_family_produces_no_finding(self):
        families = [f["family"] for f in self.result["findings"]]
        self.assertNotIn("radio-only-here", families)

    def test_extra_checker_is_reported(self):
        finding = next(f for f in self.result["findings"]
                       if f["kind"] == "checker_set")
        self.assertEqual(finding["family"], "radio-thing")
        self.assertEqual(finding["detail"]["3310"], ["tools/extra_check.py"])
        self.assertEqual(finding["detail"]["3210"], [])

    def test_differing_options_on_a_shared_checker_are_reported(self):
        finding = next(f for f in self.result["findings"]
                       if f["kind"] == "checker_options")
        self.assertEqual(finding["script"], "tools/thing_check.py")
        self.assertEqual(finding["detail"]["3210"], ["--strict"])
        self.assertEqual(finding["shared"], ["--profile"])

    def test_differing_duration_is_reported(self):
        finding = next(f for f in self.result["findings"] if f["kind"] == "seconds")
        self.assertEqual(finding["detail"], {"3210": ["20"], "3310": ["30"]})

    def test_differing_prerequisites_are_reported(self):
        finding = next(f for f in self.result["findings"]
                       if f["kind"] == "prerequisites")
        self.assertEqual(finding["detail"]["3310"], ["build"])

    def test_own_product_normalisation_is_not_a_parity_difference(self):
        """Only 3330/3410/6110 have a normalize step; that is intrinsic, not drift."""
        finding = next(f for f in self.result["findings"]
                       if f["kind"] == "prerequisites")
        self.assertNotIn("normalize-3310", finding["detail"]["3310"])

    def test_membership_covers_multi_product_families_only(self):
        self.assertIn("radio-thing", self.result["membership"])
        self.assertNotIn("radio-only-here", self.result["membership"])

    def test_report_renders(self):
        report = gate_parity_audit.render_report(self.result)
        self.assertIn("Acceptance-gate parity audit", report)
        self.assertIn("radio-thing", report)


NORMALISATION_SAMPLE = "\n".join([
    "normalize-3410:",
    "\t$(PYTHON) tools/extract_dct3_wintesla.py \\",
    "\t\t--flash-output roms/noki3410/3410f546e.fls",
    "\tcp roms/noki3210/boot_rom roms/noki3410/",
    "",
    "verify-3410-declared: normalize-3410",
    "\t@$(MAKE) run PHONE=noki3410 SECONDS=10",
    "",
    "verify-3410-transitive:",
    "\t@$(MAKE) --no-print-directory verify-3410-declared RUN_DIR=$(RUN_DIR)_a",
    "\t@$(MAKE) run PHONE=noki3410 SECONDS=10",
    "",
    "verify-3410-orphan:",
    "\t@$(MAKE) run PHONE=noki3410 SECONDS=10",
    "",
    "verify-radio-unrelated:",
    "\t@$(MAKE) run PHONE=noki3210 SECONDS=10",
    "",
])


class NormalisationReachabilityTest(unittest.TestCase):
    def setUp(self):
        self.matrix = gate_matrix.extract(NORMALISATION_SAMPLE)
        self.gaps = gate_parity_audit.normalisation_gaps(
            self.matrix, NORMALISATION_SAMPLE)

    def test_only_the_unreachable_gate_is_reported(self):
        self.assertEqual(self.gaps, {"normalize-3410": ["verify-3410-orphan"]})

    def test_a_declared_prerequisite_satisfies_reachability(self):
        self.assertNotIn("verify-3410-declared",
                         self.gaps.get("normalize-3410", []))

    def test_reachability_follows_sub_make_delegation(self):
        self.assertNotIn("verify-3410-transitive",
                         self.gaps.get("normalize-3410", []))

    def test_a_source_rom_directory_is_not_treated_as_an_output(self):
        """normalize-3410 reads roms/noki3210; that must not make 3210 depend on it."""
        self.assertNotIn("verify-radio-unrelated",
                         self.gaps.get("normalize-3410", []))


class RealMakefileTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        text = gate_matrix.gate_source().read_text()
        matrix = gate_matrix.extract(text)
        cls.result = gate_parity_audit.audit(matrix, text)

    def test_every_gate_reaches_its_rom_normalisation(self):
        self.assertEqual(self.result["normalisation_gaps"], {})

    def test_audit_runs_over_the_real_makefile(self):
        self.assertGreater(len(self.result["multi_product_families"]), 20)

    def test_every_finding_names_a_real_family(self):
        families = set(self.result["families"])
        for finding in self.result["findings"]:
            self.assertIn(finding["family"], families)

    def test_membership_products_are_consistent(self):
        for present in self.result["membership"].values():
            self.assertEqual(sorted(present), self.result["products"])


if __name__ == "__main__":
    unittest.main()
