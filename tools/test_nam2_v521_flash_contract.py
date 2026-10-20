import unittest
from pathlib import Path

from tools.nam2_v521_flash_contract import inspect, select_family, check_package


class Nam2FlashContractTest(unittest.TestCase):
    def test_last_match_wins(self):
        rows = [(1, 0xB0, 0xFFFF, 0xFFFF), (4, 0xB0, 0xEA, 0xFFFF)]
        self.assertEqual(1, select_family(rows, 0xB0, 0xD0, 0))
        self.assertEqual(4, select_family(rows, 0xB0, 0xEA, 0))

    def test_all_three_identifiers_must_match(self):
        rows = [(3, 0x89, 0x88C2, 1)]
        self.assertEqual(0, select_family(rows, 0x89, 0x88C2, 0))
        self.assertEqual(3, select_family(rows, 0x89, 0x88C2, 1))

    def test_rejects_unidentified_input(self):
        with self.assertRaisesRegex(ValueError, "unchanged acquired"):
            inspect(bytes(0x200000))

    def test_acquired_table_when_available(self):
        source = Path(__file__).resolve().parents[1] / "roms/noki2100/2100f521sharp.fls"
        if not source.exists():
            self.skipTest("acquired NAM-2 v5.21 image not present")
        result = inspect(source.read_bytes())
        self.assertEqual(10, len(result["rows"]))
        self.assertEqual((1, 0xB0, 0xFFFF, 0xFFFF), result["rows"][2])
        self.assertEqual(1, result["selected_family"])
        candidate = result["intel_c3_top_candidate"]
        self.assertEqual(3, candidate["selected_family"])
        self.assertFalse(candidate["fitted_part_proven"])

    def test_package_prefix_when_available(self):
        root = Path(__file__).resolve().parents[1]
        image = root / 'roms/noki2100/2100f521sharp.fls'
        installer = root / 'roms/archive-dct3-packages/NAM-2_MCU_v5.21.EXE'
        if not image.exists() or not installer.exists():
            self.skipTest('acquired NAM-2 inputs not present')
        report = check_package(installer.read_bytes(), image.read_bytes())
        self.assertEqual(0x1f0000, report['equal_prefix_bytes'])
        self.assertEqual(2, len(report['members']))
        self.assertFalse(report['pmm_provenance_claim'])
        with self.assertRaisesRegex(ValueError, 'installer'):
            check_package(b'invalid', image.read_bytes())
