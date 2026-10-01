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
        for address, value in ((0x473FE8, 0x200B3), (0x473FEC, 0x20033),
                               (0x473FF0, 0x200B1), (0x473FF4, 0x200B2),
                               (0x473FF8, 0x20032), (0x473FFC, 0x20031),
                               (0x474000, 0x200F1), (0x4CACC0, 0x20000),
                               (0x4CACC4, 0x168A32), (0x4CACC8, 0x168A33),
                               (0x4CACCC, 0x168A34)):
            offset = address - 0x200000
            image[offset:offset + 4] = value.to_bytes(4, "big")
        for i in range(8):
            image[contract.TABLE_OFFSET + i * 8:contract.TABLE_OFFSET + i * 8 + 6] = bytes([i]) * 6
        callers = {0x473F4C: [0x4CAA60], 0x4741C2: [0x4CAA2C]}
        with patch.object(contract, "FLASH_SHA1", hashlib.sha1(image).hexdigest()), \
                patch.object(contract, "thumb_bl_candidates", return_value=callers):
            result = contract.extract(image)
        self.assertEqual(result["patterns"], [[i] * 6 for i in range(8)])
        image[0x473FF0 - 0x200000:0x473FF4 - 0x200000] = bytes.fromhex("00020031")
        with patch.object(contract, "FLASH_SHA1", hashlib.sha1(image).hexdigest()):
            with self.assertRaisesRegex(ValueError, "GPIO literal"):
                contract.extract(image)

    def test_direct_call_scan_handles_backward_and_forward_displacement(self):
        # BL +0 then BL -4, both reach 0x200004 at distinct instruction sites.
        image = bytes.fromhex("f000f800 f7fffffe")
        self.assertEqual(contract.thumb_bl_candidates(image, (0x200004,)),
                         {0x200004: [0x200000, 0x200004]})
