from pathlib import Path
import json
import os
import shlex
import subprocess
import sys
import tempfile
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
        self.assertIn('truncate -s 0 "$(RUN_DIR)/error.log"', preparation)
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

    def test_host_gate_processes_use_the_isolated_runner(self):
        gates = (ROOT / "gates.mk").read_text()
        self.assertNotIn("--cwd $(MAME_DIR)", gates)
        self.assertNotIn("\t\t\t./mame ", gates)
        self.assertIn("tools/run_mame_isolated.py", gates)

    def test_preserved_3330_call_cleans_artifacts_without_reseeding(self):
        body = self.makefile.split(
            "verify-3330-radio-outgoing-call-host-termination:", 1
        )[1].split("\n\n", 1)[0]
        self.assertIn("prepare-run-files", body)
        self.assertIn("PRESERVE_NVRAM=1", body)
        self.assertIn("$(RUN_DIR)_provision", body)

    def test_smoke_uses_the_custom_storage_it_prepared(self):
        # Run the real recipe/isolated runner, replacing only the MAME process.
        # Keep the fixture under the checkout to exercise relative overrides.
        (ROOT / "tmp").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / "tmp") as tmp:
            root = Path(tmp)
            mame = root / "mame"
            mame.mkdir()
            fake = mame / "mame"
            fake.write_text(f"#!{sys.executable}\n" + '''
import json
import sys
from pathlib import Path
args = sys.argv[1:]
storage = Path(args[args.index('-nvram_directory') + 1])
flash = storage / 'noki5210_3/flash'
Path('invocation.json').write_text(json.dumps({
    'storage': str(storage), 'old_state_present': flash.exists()}))
flash.parent.mkdir(parents=True, exist_ok=True)
flash.write_bytes(b'new state')
''')
            fake.chmod(0o755)
            for absolute in (True, False):
                with self.subTest(absolute=absolute):
                    run = root / f"run-{absolute}"
                    storage = root / f"custom storage-{absolute}"
                    selected = storage / "noki5210_3/flash"
                    selected.parent.mkdir(parents=True)
                    selected.write_bytes(b"old custom state")
                    decoy = run / "nvram/noki5210_3/flash"
                    decoy.parent.mkdir(parents=True)
                    decoy.write_bytes(b"unselected state")
                    override = str(storage) if absolute else os.path.relpath(storage, ROOT)
                    subprocess.run(
                        ["make", "--no-print-directory", "-o", "build", "smoke",
                         "PHONE=noki5210", "BIOS=540e", f"MAME_DIR={mame}",
                         f"RUN_DIR={run}", f"RUN_NVRAM_DIR={override}", "PRESERVE_NVRAM=0"],
                        cwd=ROOT, check=True, capture_output=True, text=True)
                    invocation = json.loads((run / "invocation.json").read_text())
                    self.assertEqual(Path(invocation["storage"]).resolve(), storage.resolve())
                    self.assertFalse(invocation["old_state_present"])
                    self.assertEqual(selected.read_bytes(), b"new state")
                    self.assertEqual(decoy.read_bytes(), b"unselected state")

    def test_all_common_runners_resolve_relative_storage_before_chdir(self):
        for target in ("run", "run-prebuilt", "smoke"):
            with self.subTest(target=target):
                result = subprocess.run(
                    ["make", "--no-print-directory", "-n", "-o", "build",
                     "-o", "prepare-run-files", "-o", "frame", target,
                     "PHONE=noki5210", "BIOS=540e", "RUN_NVRAM_DIR=tmp/custom storage"],
                    cwd=ROOT, check=True, capture_output=True, text=True)
                argv = shlex.split(result.stdout.replace("\\\n", " "))
                self.assertEqual(argv[argv.index("-nvram_directory") + 1],
                                 str(ROOT / "tmp/custom storage"))


if __name__ == "__main__":
    unittest.main()
