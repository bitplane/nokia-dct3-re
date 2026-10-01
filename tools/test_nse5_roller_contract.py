import hashlib
import unittest
from unittest.mock import patch

from tools import nse5_roller_contract as contract


class RollerContractTest(unittest.TestCase):
    def test_closed_pairs_reproduce_three_distinct_rom_patterns(self):
        for phase, pattern in ((1, [1, 1, 1, 0, 1, 0]),
                               (2, [1, 0, 1, 1, 0, 1]),
                               (3, [0, 1, 0, 1, 1, 1])):
            self.assertEqual(contract.probe_pattern(phase), pattern)

    def test_restored_drive_detects_both_next_phases(self):
        for previous in (1, 2, 3):
            for current in (1, 2, 3):
                levels = contract.contact_levels(current, previous - 1)
                self.assertEqual(contract.fast_phase(levels, previous), current)

    def test_invalid_fast_samples_retain_previous_phase(self):
        for sample in ((0, 0, 0), (1, 1, 1), (0, 1, 1), (1, 0, 1), (1, 1, 0)):
            self.assertEqual(contract.fast_phase(sample, 2), 2)

    def test_wrong_flash_rejected(self):
        with self.assertRaises(ValueError):
            contract.extract(b"")

    def test_initialization_record_and_all_eight_rows(self):
        image = bytearray(contract.TABLE_OFFSET + 62)
        image[contract.TABLE_OFFSET - 8:contract.TABLE_OFFSET] = bytes.fromhex(
            "0000003e00168a3c")
        for i in range(8):
            image[contract.TABLE_OFFSET + i * 8:contract.TABLE_OFFSET + i * 8 + 6] = bytes([i]) * 6
        with patch.object(contract, "FLASH_SHA1", hashlib.sha1(image).hexdigest()):
            result = contract.extract(image)
        self.assertEqual(result["patterns"], [[i] * 6 for i in range(8)])
