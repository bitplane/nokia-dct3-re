from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class AcceptanceRunIsolationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.makefile = ((ROOT / "Makefile").read_text() + "\n"
                     + (ROOT / "gates.mk").read_text())

    def test_common_run_preparation_owns_generated_artifacts(self):
        preparation = self.makefile.split("prepare-run-files:", 1)[1].split(
            "prepare-run-nvram:", 1
        )[0]
        self.assertIn('mkdir -p "$(RUN_DIR)"', preparation)
        self.assertIn("nokia_dct3_lcdmirror_*.pgm", preparation)
        self.assertIn('truncate -s 0 "$(MAME_DIR)/error.log"', preparation)
        self.assertIn('"$(PRESERVE_NVRAM)" != "1"', preparation)

    def test_direct_host_targets_use_common_preparation(self):
        for target in (
            "verify-radio-outgoing-call-host-adapter",
            "verify-radio-outgoing-call-host-local-end",
            "verify-radio-outgoing-call-host-termination",
            "verify-radio-outgoing-call-host-alerting-termination",
            "verify-radio-outgoing-call-host-media",
            "verify-radio-outgoing-call-host-reconnect",
            "verify-radio-outgoing-call-host-alerting-reconnect",
            "verify-radio-outgoing-call-host-media-restore",
            "verify-radio-outgoing-call-host-release-restore",
            "verify-radio-outgoing-call-host-two-calls",
        ):
            body = self.makefile.split(f"{target}:", 1)[1].split("\n\n", 1)[0]
            self.assertIn("prepare_host_run", body, target)

    def test_preserved_3330_call_cleans_artifacts_without_reseeding(self):
        body = self.makefile.split(
            "verify-3330-radio-outgoing-call-host-termination:", 1
        )[1].split("\n\n", 1)[0]
        self.assertIn("prepare-run-files", body)
        self.assertIn("PRESERVE_NVRAM=1", body)
        self.assertIn("$(RUN_DIR)_provision", body)


if __name__ == "__main__":
    unittest.main()
