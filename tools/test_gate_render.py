#!/usr/bin/env python3
"""Tests for rendering gate records back into their commands."""

import copy
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import gate_matrix
import gate_render


SAMPLE = "\n".join([
    "verify-example:",
    "\t@$(MAKE) --no-print-directory run $(NOKI3410_RUN) \\",
    "\t\tRUN_DIR=$(RUN_DIR) SECONDS=40 RUN_VERBOSE=1",
    "\tcp $(MAME_DIR)/error.log $(RUN_DIR)/error.log",
    "\t$(PYTHON) tools/thing_check.py $(RUN_DIR)/error.log --profile nhm2",
    "\t@echo \"OK — example\"",
    "",
])


class RenderStepTest(unittest.TestCase):
    def setUp(self):
        matrix = gate_matrix.extract(SAMPLE)
        self.gate = matrix["gates"][0]

    def test_run_step_preserves_bare_token_position(self):
        """$(NOKI3410_RUN) expands to assignments; its position decides overrides."""
        rendered = gate_render.render_step(self.gate["steps"][0])
        self.assertEqual(
            rendered,
            "@$(MAKE) --no-print-directory run $(NOKI3410_RUN) "
            "RUN_DIR=$(RUN_DIR) SECONDS=40 RUN_VERBOSE=1")

    def test_silent_prefix_is_preserved(self):
        self.assertTrue(gate_render.render_step(self.gate["steps"][0]).startswith("@"))
        self.assertFalse(gate_render.render_step(self.gate["steps"][1]).startswith("@"))

    def test_check_step_preserves_interpreter_and_arguments(self):
        rendered = gate_render.render_step(self.gate["steps"][2])
        self.assertEqual(
            rendered,
            "$(PYTHON) tools/thing_check.py $(RUN_DIR)/error.log --profile nhm2")

    def test_gate_renders_to_its_own_commands(self):
        self.assertEqual(gate_render.check_gate(self.gate), [])


class TokeniserTest(unittest.TestCase):
    def test_make_expansion_is_one_token(self):
        tokens = gate_matrix._split_arguments(
            '$(PYTHON) tool.py "$(CAPTURE)" $(if $(RAW),--raw "$(RAW)")')
        self.assertEqual(tokens[-1], '$(if $(RAW),--raw "$(RAW)")')

    def test_quoted_run_is_one_token(self):
        tokens = gate_matrix._split_arguments("cmd RUN_ENV='a b c' next")
        self.assertEqual(tokens[1], "RUN_ENV='a b c'")


class CheckIsNotVacuousTest(unittest.TestCase):
    """The equivalence check must actually be able to fail."""

    def test_a_dropped_argument_is_detected(self):
        matrix = gate_matrix.extract(SAMPLE)
        gate = copy.deepcopy(matrix["gates"][0])
        gate["steps"][2]["arguments"].pop()
        differences = gate_render.check_gate(gate)
        self.assertTrue(differences)

    def test_a_dropped_step_is_detected(self):
        matrix = gate_matrix.extract(SAMPLE)
        gate = copy.deepcopy(matrix["gates"][0])
        gate["steps"].pop()
        differences = gate_render.check_gate(gate)
        self.assertTrue(differences)

    def test_a_changed_silent_prefix_is_detected(self):
        matrix = gate_matrix.extract(SAMPLE)
        gate = copy.deepcopy(matrix["gates"][0])
        gate["steps"][0]["silent"] = False
        differences = gate_render.check_gate(gate)
        self.assertTrue(differences)


class RealMakefileTest(unittest.TestCase):
    """The Phase 1 contract for the structured half."""

    @classmethod
    def setUpClass(cls):
        cls.matrix = gate_matrix.extract(gate_matrix.gate_source().read_text())
        cls.result = gate_render.check_all(cls.matrix)

    def test_every_structured_gate_reproduces_its_commands(self):
        self.assertEqual(
            self.result["failures"], {},
            "structured gates whose typed data loses information")

    def test_the_structured_half_is_not_empty(self):
        self.assertGreater(self.result["structured"], 100)


if __name__ == "__main__":
    unittest.main()
