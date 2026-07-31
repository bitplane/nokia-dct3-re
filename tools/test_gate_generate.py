#!/usr/bin/env python3
"""Tests for generating the acceptance-gate rules."""

import pathlib
import re
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import gate_generate
import gate_matrix


SAMPLE = "\n".join([
    "help:",
    "\t@echo help",
    "",
    "PYTHON := python3",
    "",
    "verify-plain: build",
    "\t@$(MAKE) --no-print-directory run PHONE=noki3210 SECONDS=20",
    "\tcp $(MAME_DIR)/error.log $(RUN_DIR)/error.log",
    "\t$(PYTHON) tools/thing_check.py $(RUN_DIR)/error.log --profile nse8",
    "",
    "other-target:",
    "\techo untouched",
    "",
    "verify-loop: ERASED=12345",
    "verify-loop:",
    "\t@set -e; \\",
    "\tfor boundary in one two; do \\",
    "\t\t$(PYTHON) tools/loop_check.py \"$$boundary\"; \\",
    "\tdone",
    "",
])


class GenerateTest(unittest.TestCase):
    def setUp(self):
        self.matrix = gate_matrix.extract(SAMPLE)
        self.generated = gate_generate.generate(self.matrix)

    def test_phony_lists_every_gate(self):
        phony = next(line for line in self.generated.split("\n")
                     if line.startswith(".PHONY:"))
        self.assertIn("verify-plain", phony)
        self.assertIn("verify-loop", phony)

    def test_structured_gate_is_emitted_one_command_per_line(self):
        self.assertIn(
            "\t@$(MAKE) --no-print-directory run PHONE=noki3210 SECONDS=20",
            self.generated)
        self.assertIn(
            "\t$(PYTHON) tools/thing_check.py $(RUN_DIR)/error.log --profile nse8",
            self.generated)

    def test_prerequisites_and_variables_are_preserved(self):
        self.assertIn("verify-plain: build", self.generated)
        self.assertIn("verify-loop: ERASED=12345", self.generated)

    def test_shell_gate_is_copied_verbatim_and_marked(self):
        self.assertIn("# shell: embedded shell control flow", self.generated)
        self.assertIn("\tfor boundary in one two; do \\", self.generated)


class MakefileTrimTest(unittest.TestCase):
    def setUp(self):
        self.matrix = gate_matrix.extract(SAMPLE)
        self.trimmed = gate_generate.makefile_without_gates(self.matrix)

    def test_gate_rules_are_removed(self):
        self.assertNotIn("verify-plain", self.trimmed)
        self.assertNotIn("verify-loop", self.trimmed)

    def test_non_gate_content_is_kept(self):
        self.assertIn("other-target:", self.trimmed)
        self.assertIn("PYTHON := python3", self.trimmed)

    def test_include_is_appended(self):
        self.assertTrue(self.trimmed.rstrip().endswith("include gates.mk"))

    def test_default_goal_is_unchanged(self):
        """The first target sets the default goal; it must remain `help`."""
        first = next(line for line in self.trimmed.split("\n")
                     if re.match(r"^[a-zA-Z][a-zA-Z0-9_-]*:", line))
        self.assertTrue(first.startswith("help:"))

    def test_include_comes_after_the_first_target(self):
        lines = self.trimmed.split("\n")
        include = lines.index("include gates.mk")
        first_target = next(i for i, line in enumerate(lines)
                            if re.match(r"^[a-zA-Z][a-zA-Z0-9_-]*:", line))
        self.assertGreater(include, first_target)


class EquivalenceTest(unittest.TestCase):
    def setUp(self):
        self.matrix = gate_matrix.extract(SAMPLE)

    def test_faithful_generation_passes(self):
        generated = gate_generate.generate(self.matrix)
        trimmed = gate_generate.makefile_without_gates(self.matrix)
        result = gate_generate.verify_equivalence(self.matrix, generated, trimmed)
        self.assertEqual(result["problems"], {})

    def test_a_dropped_gate_is_detected(self):
        generated = gate_generate.generate(self.matrix)
        broken = generated.replace(
            "\t$(PYTHON) tools/thing_check.py $(RUN_DIR)/error.log --profile nse8\n",
            "")
        trimmed = gate_generate.makefile_without_gates(self.matrix)
        result = gate_generate.verify_equivalence(self.matrix, broken, trimmed)
        self.assertIn("verify-plain", result["problems"])

    def test_a_changed_prerequisite_is_detected(self):
        generated = gate_generate.generate(self.matrix)
        broken = generated.replace("verify-plain: build", "verify-plain:")
        trimmed = gate_generate.makefile_without_gates(self.matrix)
        result = gate_generate.verify_equivalence(self.matrix, broken, trimmed)
        self.assertIn("verify-plain", result["problems"])


class RealMakefileTest(unittest.TestCase):
    """The generated rules must specify exactly today's commands."""

    @classmethod
    def setUpClass(cls):
        text = gate_matrix.gate_source().read_text()
        cls.matrix = gate_matrix.extract(text)
        cls.generated = gate_generate.generate(cls.matrix)
        cls.trimmed = gate_generate.makefile_without_gates(cls.matrix)

    def test_every_gate_survives_generation_unchanged(self):
        result = gate_generate.verify_equivalence(
            self.matrix, self.generated, self.trimmed)
        self.assertEqual(result["problems"], {})

    def test_regenerating_from_the_rules_preserves_every_command(self):
        """Re-extracting gates.mk and regenerating must not change any command.

        Byte identity is not the contract here: gates.mk is generated from
        gates.json, which carries the press-profile table that a re-extraction
        of gates.mk cannot recover. The invariant that does hold either way is
        that no gate's command sequence changes. Byte identity against the
        authored data is asserted by AuthoredDataTest.
        """
        if not gate_generate.GATES_MK.exists():
            self.skipTest("gates.mk not generated yet")
        result = gate_generate.verify_equivalence(
            self.matrix, self.generated, "")
        self.assertEqual(result["problems"], {})


if __name__ == "__main__":
    unittest.main()


class AuthoredDataTest(unittest.TestCase):
    """gates.json is the source; gates.mk is its generated form."""

    @classmethod
    def setUpClass(cls):
        import json
        if not gate_generate.GATES_JSON.exists():
            raise unittest.SkipTest("gates.json not emitted yet")
        cls.data = json.loads(gate_generate.GATES_JSON.read_text())

    def test_data_regenerates_the_committed_rules_exactly(self):
        rebuilt = gate_generate.generate(gate_generate.from_data(self.data))
        self.assertEqual(rebuilt, gate_generate.GATES_MK.read_text())

    def test_every_gate_is_either_typed_or_declared_shell(self):
        for record in self.data["gates"]:
            typed = "steps" in record
            shell = "unstructured" in record and "recipe" in record
            self.assertTrue(typed or shell, record["name"])

    def test_round_trip_through_data_preserves_gate_count(self):
        matrix = gate_matrix.extract(gate_matrix.gate_source().read_text())
        self.assertEqual(len(self.data["gates"]), len(matrix["gates"]))


class PressProfileTest(unittest.TestCase):
    """Named key press shapes must be defined wherever they are used."""

    @classmethod
    def setUpClass(cls):
        import json
        if not gate_generate.GATES_JSON.exists():
            raise unittest.SkipTest("gates.json not emitted yet")
        cls.data = json.loads(gate_generate.GATES_JSON.read_text())
        cls.profiles = cls.data.get("press_profiles", {})
        cls.generated = gate_generate.GATES_MK.read_text()

    def test_every_referenced_profile_is_defined(self):
        used = set(re.findall(r"\$\((DCT3_PRESS_[0-9_]+)\)", self.generated))
        self.assertTrue(used, "no press profile is referenced")
        self.assertEqual(used - set(self.profiles), set())

    def test_every_defined_profile_is_used(self):
        used = set(re.findall(r"\$\((DCT3_PRESS_[0-9_]+)\)", self.generated))
        self.assertEqual(set(self.profiles) - used, set())

    def test_profile_value_matches_its_name(self):
        """DCT3_PRESS_220_280 must be a 220 ms press with a 280 ms gap."""
        for name, value in self.profiles.items():
            duration, gap = name[len("DCT3_PRESS_"):].split("_")
            self.assertIn(f"DURATION_MS={duration}", value)
            self.assertIn(f"GAP_MS={gap}", value)

    def test_profiles_are_emitted_before_use(self):
        lines = self.generated.split("\n")
        definition = next(i for i, line in enumerate(lines)
                          if line.startswith("DCT3_PRESS_"))
        first_use = next(i for i, line in enumerate(lines)
                         if "$(DCT3_PRESS_" in line)
        self.assertLess(definition, first_use)


class ProductRunTest(unittest.TestCase):
    """Per-product run identities must be defined wherever they are used."""

    @classmethod
    def setUpClass(cls):
        import json
        if not gate_generate.GATES_JSON.exists():
            raise unittest.SkipTest("gates.json not emitted yet")
        cls.data = json.loads(gate_generate.GATES_JSON.read_text())
        cls.runs = cls.data.get("product_runs", {})
        cls.generated = gate_generate.GATES_MK.read_text()

    def test_every_referenced_identity_is_defined(self):
        used = set(re.findall(r"\$\((DCT3_RUN_[0-9A-Z_]+)\)", self.generated))
        self.assertTrue(used, "no product run identity is referenced")
        self.assertEqual(used - set(self.runs), set())

    def test_every_defined_identity_is_used(self):
        used = set(re.findall(r"\$\((DCT3_RUN_[0-9A-Z_]+)\)", self.generated))
        self.assertEqual(set(self.runs) - used, set())

    def test_identity_names_its_own_handset(self):
        for name, value in self.runs.items():
            model = name[len("DCT3_RUN_"):].split("_")[0]
            self.assertIn(f"PHONE=noki{model}", value)

    def test_no_recipe_still_inlines_a_named_identity(self):
        """A literal left in a recipe would mean the table is half applied.

        Target-specific variable assignments are excluded deliberately. A line
        such as `verify: PHONE=noki3210` sets a variable for that target; it is
        not a run token, and substituting a variable reference there would turn
        the assignment into a prerequisite.
        """
        recipes = [line for line in self.generated.split("\n")
                   if line.startswith("\t")]
        for value in self.runs.values():
            offenders = [line for line in recipes if value in line]
            self.assertEqual(offenders, [], f"{value} still appears inline")

    def test_superseded_single_product_variable_is_gone(self):
        self.assertNotIn("NOKI3410_RUN", self.generated)


class ShellGuardTest(unittest.TestCase):
    """The shared EEPROM restore guard."""

    @classmethod
    def setUpClass(cls):
        import json
        if not gate_generate.GATES_JSON.exists():
            raise unittest.SkipTest("gates.json not emitted yet")
        cls.data = json.loads(gate_generate.GATES_JSON.read_text())
        cls.guards = cls.data.get("shell_guards", {})
        cls.generated = gate_generate.GATES_MK.read_text()

    def test_guards_are_deferred_not_immediate(self):
        """`:=` would freeze $(EEPROM_BASENAME) at the default $(BIOS).

        The guard restores the EEPROM for whichever firmware revision the gate
        runs. Immediate assignment resolves the filename once, at definition
        time, so a v5.01 gate would restore the v6.00 image instead.
        """
        for name in self.guards:
            self.assertIn(f"{name} = ", self.generated)
            self.assertNotIn(f"{name} := ", self.generated)

    def test_every_referenced_guard_is_defined(self):
        used = set(re.findall(r"\$\((DCT3_EEPROM_GUARD[A-Z0-9_]*)\)", self.generated))
        self.assertTrue(used, "no shell guard is referenced")
        self.assertEqual(used - set(self.guards), set())

    def test_no_recipe_still_inlines_the_guard(self):
        recipes = [line for line in self.generated.split("\n")
                   if line.startswith("\t")]
        inline = [line for line in recipes if "restore_default() {" in line]
        self.assertEqual(inline, [])

    def test_guarded_gates_remain_classified_as_shell(self):
        """A `;`-chained blob must not be mistaken for a single run step."""
        matrix = gate_matrix.extract(gate_matrix.gate_source().read_text())
        for gate in matrix["gates"]:
            recipe = "\n".join(gate["verbatim"]["recipe"])
            if "DCT3_EEPROM_GUARD" in recipe:
                self.assertIn("unstructured", gate, gate["name"])


class CaptureRedundancyTest(unittest.TestCase):
    """A `*-captured` run target already copies error.log."""

    @classmethod
    def setUpClass(cls):
        import json
        if not gate_generate.GATES_JSON.exists():
            raise unittest.SkipTest("gates.json not emitted yet")
        cls.data = json.loads(gate_generate.GATES_JSON.read_text())

    def test_no_gate_captures_the_log_twice(self):
        captured = ("run-captured", "run-prebuilt-captured")
        for gate in self.data["gates"]:
            steps = gate.get("steps", [])
            for index, step in enumerate(steps[:-1]):
                if step["step"] == "run" and step.get("run_target") in captured:
                    following = steps[index + 1]
                    self.assertNotEqual(
                        following["step"], "capture_log",
                        f"{gate['name']} copies error.log after "
                        f"{step['run_target']}, which already does it")
