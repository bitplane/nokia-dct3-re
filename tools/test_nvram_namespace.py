from pathlib import Path
import re
import subprocess
import tempfile
import unittest

from tools.dct3_products import DRIVER, nvram_system


ROOT = Path(__file__).resolve().parents[1]
STORES = ("flash", "sim_card", "ccont", "gsm_network", "eeprom")


class NvramNamespaceTests(unittest.TestCase):
    def test_all_declared_machine_defaults_and_bios_choices(self):
        source = DRIVER.read_text()
        systems = re.findall(r"^SYST\(\s*\d+\s*,\s*(\w+)\s*,", source, re.M)
        for machine in systems:
            with self.subTest(machine=machine):
                self.assertEqual(nvram_system(machine), machine)
                self.assertEqual(nvram_system(machine, "default"), machine)
        count = 0
        for machine, body in re.findall(r"ROM_START\(\s*(\w+)\s*\)(.*?)ROM_END", source, re.S):
            entries = re.findall(r'ROM_SYSTEM_BIOS\((\d+), "([^"]+)"', body)
            for index, name in entries:
                expected = machine if index == entries[0][0] else f"{machine}_{index}"
                with self.subTest(machine=machine, bios=name):
                    self.assertEqual(nvram_system(machine, name), expected)
                    self.assertEqual(nvram_system(machine, index), expected)
                    self.assertEqual(nvram_system(machine, name.upper()), expected)
                count += 1
        self.assertGreater(count, 30)
        self.assertEqual(nvram_system("nam2c3", "521sharp"), "nam2c3_1")
        self.assertEqual(nvram_system("nse1pcm"), "nse1pcm")

    def test_explicit_nonfirst_default_and_sparse_indices(self):
        source = '''
SYST(2000, example, 0, 0, example, example, state, empty_init, "", "", 0)
ROM_START(example)
    // ROM_SYSTEM_BIOS(0, "ignored", "not a declaration")
    ROM_SYSTEM_BIOS(2, "early", "Early")
    ROM_SYSTEM_BIOS(7, "late", "Late")
    ROM_DEFAULT_BIOS("late")
ROM_END
'''
        self.assertEqual(nvram_system("example", source=source), "example")
        self.assertEqual(nvram_system("example", "late", source=source), "example")
        self.assertEqual(nvram_system("example", "2", source=source), "example_2")
        self.assertEqual(nvram_system("example", "early", source=source), "example_2")

    def test_unknown_selections_fail_closed(self):
        for machine, bios in (("no_such_phone", ""), ("noki2100", "bad"),
                              ("noki5110", "530")):
            with self.subTest(machine=machine, bios=bios), self.assertRaises(ValueError):
                nvram_system(machine, bios)

    def test_fresh_and_preserved_runs_use_selected_bios_storage(self):
        for machine, bios, directory in (
            ("noki2100", "521sharp", "noki2100_1"),
            ("nam2c3", "1", "nam2c3_1"),
            ("noki3310", "579", "noki3310_1"),
            ("noki5210", "520", "noki5210_2"),
            ("noki5190", "671b", "noki5190_1"),
            ("noki6110", "548r4", "noki6110_2"),
            ("noki5110", "", "noki5110"),
        ):
            with self.subTest(machine=machine, bios=bios), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                storage = root / "nvram" / directory
                storage.mkdir(parents=True)
                for store in STORES:
                    (storage / store).write_bytes(b"old state")
                other = root / "nvram" / "unselected_bios" / "flash"
                other.parent.mkdir()
                other.write_bytes(b"keep")
                base = ["make", "--no-print-directory", "prepare-run-files",
                        f"PHONE={machine}", f"BIOS={bios}", f"RUN_DIR={root / 'run'}",
                        f"RUN_NVRAM_DIR={root / 'nvram'}"]
                subprocess.run(base + ["PRESERVE_NVRAM=1"], cwd=ROOT,
                               check=True, capture_output=True, text=True)
                self.assertTrue(all((storage / name).read_bytes() == b"old state" for name in STORES))
                subprocess.run(base + ["PRESERVE_NVRAM=0"], cwd=ROOT,
                               check=True, capture_output=True, text=True)
                self.assertTrue(all(not (storage / name).exists() for name in STORES))
                self.assertEqual(other.read_bytes(), b"keep")

    def test_invalid_bios_does_not_delete_existing_state(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            storage = root / "nvram/noki2100/flash"
            storage.parent.mkdir(parents=True)
            storage.write_bytes(b"keep")
            result = subprocess.run(
                ["make", "--no-print-directory", "prepare-run-files", "PHONE=noki2100",
                 "BIOS=invalid", f"RUN_DIR={root / 'run'}", f"RUN_NVRAM_DIR={root / 'nvram'}"],
                cwd=ROOT, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(storage.read_bytes(), b"keep")
            self.assertIn("Cannot resolve NVRAM namespace", result.stderr)


if __name__ == "__main__":
    unittest.main()
