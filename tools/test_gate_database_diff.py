#!/usr/bin/env python3
"""Tests for the independent make-database comparison."""

import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import gate_database_diff as diff


DATABASE = "\n".join([
    "# Variables",
    "",
    "PYTHON = python3",
    "RUN_DIR = run",
    "PRESS = DURATION=220 GAP=280",
    "",
    "# Files",
    "",
    "verify-example: build",
    "#  Phony target (prerequisite of .PHONY).",
    "\t@$(MAKE) run RUN_DIR=$(RUN_DIR) \\",
    "\t\tSECONDS=20",
    "\t$(PYTHON) tools/check.py DURATION=220 GAP=280",
    "",
    "other: ",
    "\techo hello",
    "",
])

MIGRATED = DATABASE.replace(
    "\t$(PYTHON) tools/check.py DURATION=220 GAP=280",
    "\t$(PYTHON) tools/check.py $(PRESS)")


class ParseTest(unittest.TestCase):
    def setUp(self):
        self.targets = diff.parse_database(DATABASE)

    def test_target_recipe_and_prerequisites_are_read(self):
        entry = self.targets["verify-example"]
        self.assertEqual(entry["prerequisites"], ["build"])
        self.assertEqual(len(entry["recipe"]), 3)

    def test_phony_status_is_read(self):
        self.assertTrue(self.targets["verify-example"]["phony"])
        self.assertFalse(self.targets["other"]["phony"])

    def test_variables_are_read(self):
        variables = diff.parse_variables(DATABASE)
        self.assertEqual(variables["PYTHON"], "python3")
        self.assertEqual(variables["PRESS"], "DURATION=220 GAP=280")


class LogicalCommandTest(unittest.TestCase):
    def test_continuations_are_joined(self):
        commands = diff.logical_commands(
            ["\t@$(MAKE) run \\", "\t\tSECONDS=20", "\tcp a b"])
        self.assertEqual(commands, ["@$(MAKE) run SECONDS=20", "cp a b"])


class ExpandTest(unittest.TestCase):
    def test_simple_reference_is_expanded(self):
        self.assertEqual(diff.expand("$(PYTHON) x", {"PYTHON": "python3"}),
                         "python3 x")

    def test_unknown_name_is_left_alone(self):
        self.assertEqual(diff.expand("$(NOPE) x", {}), "$(NOPE) x")

    def test_function_calls_are_not_evaluated(self):
        """The `if` is not resolved, though references inside it are expanded.

        That is enough for comparison: both sides are treated identically, so
        an unevaluated function appears the same before and after.
        """
        self.assertEqual(diff.expand("$(if $(RAW),--raw)", {"RAW": "1"}),
                         "$(if 1,--raw)")
        self.assertEqual(diff.expand("$(if $(RAW),--raw)", {}),
                         "$(if $(RAW),--raw)")

    def test_nested_references_resolve(self):
        self.assertEqual(diff.expand("$(A)", {"A": "$(B)", "B": "done"}), "done")


class CompareTest(unittest.TestCase):
    """Naming a repeated literal must be invisible expanded, visible raw."""

    def test_unexpanded_comparison_sees_the_naming(self):
        result = diff.compare(diff.parse_database(DATABASE),
                              diff.parse_database(MIGRATED))
        self.assertIn("verify-example", result["problems"])

    def test_expanded_comparison_sees_no_difference(self):
        result = diff.compare(diff.parse_database(DATABASE),
                              diff.parse_database(MIGRATED),
                              variables_before=diff.parse_variables(DATABASE),
                              variables_after=diff.parse_variables(MIGRATED))
        self.assertEqual(result["problems"], {})

    def test_a_dropped_command_is_reported_even_expanded(self):
        broken = MIGRATED.replace(
            "\t$(PYTHON) tools/check.py $(PRESS)\n", "")
        result = diff.compare(diff.parse_database(DATABASE),
                              diff.parse_database(broken),
                              variables_before=diff.parse_variables(DATABASE),
                              variables_after=diff.parse_variables(broken))
        self.assertIn("verify-example", result["problems"])

    def test_a_changed_prerequisite_is_reported(self):
        broken = MIGRATED.replace("verify-example: build", "verify-example:")
        result = diff.compare(diff.parse_database(DATABASE),
                              diff.parse_database(broken))
        self.assertIn("verify-example", result["problems"])

    def test_a_lost_phony_is_reported(self):
        broken = MIGRATED.replace(
            "#  Phony target (prerequisite of .PHONY).\n", "")
        result = diff.compare(diff.parse_database(DATABASE),
                              diff.parse_database(broken))
        self.assertIn("verify-example", result["problems"])

    def test_a_missing_target_is_reported(self):
        result = diff.compare(diff.parse_database(DATABASE), {})
        self.assertEqual(result["problems"]["verify-example"],
                         ["missing after the change"])


if __name__ == "__main__":
    unittest.main()
