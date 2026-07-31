#!/usr/bin/env python3
"""Tests for the acceptance-gate matrix extraction."""

import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import gate_matrix


SAMPLE = "\n".join([
    "# leading comment",
    "OTHER := value",
    "",
    "verify-example: ERASED_IDENTITY_SECURITY_CODE=12345",
    "verify-example: build",
    "\t@$(MAKE) --no-print-directory run PHONE=noki3310 BIOS=639 \\",
    "\t\tRUN_DIR=$(RUN_DIR) SECONDS=35 RUN_VERBOSE=1",
    "\tcp $(MAME_DIR)/error.log $(RUN_DIR)/error.log",
    "\t$(PYTHON) tools/radio_registration_trace_check.py \\",
    "\t\t$(RUN_DIR)/error.log --profile nhm5",
    "",
    "not-a-gate:",
    "\techo hello",
    "",
    "verify-loop:",
    "\t@set -e; \\",
    "\tfor boundary in one two; do \\",
    "\t\t$(PYTHON) tools/radio_paging_trace_check.py \"$$out/error.log\"; \\",
    "\tdone",
    "",
])


class RoundTripTest(unittest.TestCase):
    def test_sample_round_trips(self):
        matrix = gate_matrix.check_round_trip(SAMPLE)
        self.assertEqual(gate_matrix.render(matrix), SAMPLE)

    def test_real_makefile_round_trips(self):
        text = gate_matrix.gate_source().read_text()
        matrix = gate_matrix.check_round_trip(text)
        self.assertEqual(gate_matrix.render(matrix), text)

    def test_round_trip_failure_is_reported(self):
        matrix = gate_matrix.extract(SAMPLE)
        matrix["segments"][0]["lines"][0] = "# tampered"
        with self.assertRaises(gate_matrix.RoundTripError):
            if gate_matrix.render(matrix) != SAMPLE:
                raise gate_matrix.RoundTripError("differs")


class ExtractionTest(unittest.TestCase):
    def setUp(self):
        self.matrix = gate_matrix.extract(SAMPLE)
        self.gates = {gate["name"]: gate for gate in self.matrix["gates"]}

    def test_only_verify_targets_are_gates(self):
        self.assertEqual(sorted(self.gates), ["verify-example", "verify-loop"])

    def test_declarations_split_into_variables_and_prerequisites(self):
        gate = self.gates["verify-example"]
        self.assertEqual(gate["variables"],
                         {"ERASED_IDENTITY_SECURITY_CODE": "12345"})
        self.assertEqual(gate["prerequisites"], ["build"])

    def test_run_step_captures_parameters(self):
        run = self.gates["verify-example"]["steps"][0]
        self.assertEqual(run["step"], "run")
        self.assertEqual(run["run_target"], "run")
        self.assertEqual(run["parameters"]["PHONE"], "noki3310")
        self.assertEqual(run["parameters"]["BIOS"], "639")
        self.assertEqual(run["parameters"]["SECONDS"], "35")

    def test_check_step_captures_script_and_options(self):
        check = self.gates["verify-example"]["steps"][2]
        self.assertEqual(check["step"], "check")
        self.assertEqual(check["script"], "tools/radio_registration_trace_check.py")
        self.assertEqual(check["options"], ["--profile"])

    def test_capture_log_step_recognised(self):
        self.assertEqual(self.gates["verify-example"]["steps"][1]["step"],
                         "capture_log")

    def test_control_flow_recipe_is_not_decomposed(self):
        gate = self.gates["verify-loop"]
        self.assertEqual(gate["unstructured"], "embedded shell control flow")
        self.assertEqual(gate["steps"], [])

    def test_control_flow_recipe_still_surfaces_its_checkers(self):
        gate = self.gates["verify-loop"]
        self.assertEqual(gate["scripts_mentioned"],
                         ["tools/radio_paging_trace_check.py"])


class RealMakefileTest(unittest.TestCase):
    """Guard the properties the parity audit depends on."""

    @classmethod
    def setUpClass(cls):
        cls.matrix = gate_matrix.extract(gate_matrix.gate_source().read_text())
        cls.gates = cls.matrix["gates"]

    def test_every_gate_name_is_unique(self):
        names = [gate["name"] for gate in self.gates]
        self.assertEqual(len(names), len(set(names)))

    def test_no_gate_asserts_nothing(self):
        """A gate with no assertion mechanism cannot fail, so it proves nothing."""
        blind = [gate["name"] for gate in self.gates if not gate["asserts_via"]]
        self.assertEqual(blind, [], f"gates that cannot fail: {blind}")

    def test_assertion_kinds_are_known(self):
        known = {"checker", "delegate", "native_test", "script", "prerequisite",
                 "macro", "shell_assertion"}
        for gate in self.gates:
            self.assertTrue(set(gate["asserts_via"]) <= known,
                            f"{gate['name']}: {gate['asserts_via']}")

    def test_public_matrix_drops_verbatim_text(self):
        public = gate_matrix.public_matrix(self.matrix)
        self.assertTrue(all("verbatim" not in gate for gate in public["gates"]))


if __name__ == "__main__":
    unittest.main()
