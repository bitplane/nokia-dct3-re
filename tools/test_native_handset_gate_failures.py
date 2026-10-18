"""Execute the Makefile's fault guards; this tests gate wiring, not DSP behavior."""

from pathlib import Path
import re
import subprocess
import tempfile
import unittest


class NativeHandsetGateFailuresTest(unittest.TestCase):
    def test_fault_guards_abort_the_recipe(self):
        makefile = (Path(__file__).resolve().parents[1] / "Makefile").read_text()
        for target in ("verify-5110-menu", "verify-5110-save-state",
                       "check-c54x-rom4-coherent", "check-c54x-rom4-codec-tone"):
            recipe = makefile.split(target + ": build\n", 1)[1].split("\n\n", 1)[0]
            guard = re.search(r"if grep -Eq .*?error.log; then.*?fi;", recipe, re.S)
            self.assertIsNotNone(guard, target)
            script = "set -eu; " + guard.group(0).replace("\\\n", " ") + " echo survived"
            for fault in ("", "rom4_reset_request: reason=00000004",
                          "TMS320C54x illegal instruction",
                          "unsupported staged C54x form fe44",
                          "staged ADDM BRC is unsupported",
                          "unimplemented C54x operation"):
                with self.subTest(target=target, fault=fault), tempfile.TemporaryDirectory() as directory:
                    (Path(directory) / "error.log").write_text(fault)
                    result = subprocess.run(["sh", "-c", script], cwd=directory,
                                            text=True, capture_output=True)
                    self.assertEqual(result.returncode, 1 if fault else 0)
                    self.assertEqual("survived" in result.stdout, not bool(fault))

    def test_tone_lua_guard_aborts_the_recipe(self):
        makefile = (Path(__file__).resolve().parents[1] / "Makefile").read_text()
        recipe = makefile.split("check-c54x-rom4-codec-tone: build\n", 1)[1].split("\n\n", 1)[0]
        guard = re.search(r"if grep -Eiq .*?output.log; then.*?fi;", recipe, re.S)
        self.assertIsNotNone(guard)
        script = "set -eu; " + guard.group(0).replace("\\\n", " ") + " echo survived"
        for message in ("ordinary output", "Lua error", "ERROR in Lua callback"):
            with self.subTest(message=message), tempfile.TemporaryDirectory() as directory:
                (Path(directory) / "output.log").write_text(message)
                result = subprocess.run(["sh", "-c", script], cwd=directory, text=True, capture_output=True)
                self.assertEqual(result.returncode, 0 if message == "ordinary output" else 1)

    def test_no_ineffective_negated_grep_remains(self):
        makefile = (Path(__file__).resolve().parents[1] / "Makefile").read_text()
        self.assertNotRegex(makefile, r"!\s+grep\b")


if __name__ == "__main__":
    unittest.main()
