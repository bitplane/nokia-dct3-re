#!/usr/bin/env python3
"""Unit tests for the games mapping tools: BL decoding, closure/boundary split, worklist ranking."""
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import call_closure
import thumb_entries  # noqa: F401  (import check)

FLASH = 0x200000


def bl(from_addr, to_addr):
    """Encode a Thumb-1 BL pair (little-endian halfwords)."""
    off = (to_addr - (from_addr + 4)) >> 1
    hi = 0xF000 | ((off >> 11) & 0x7FF)
    lo = 0xF800 | (off & 0x7FF)
    return hi.to_bytes(2, "little") + lo.to_bytes(2, "little")


class BlDecodeTests(unittest.TestCase):
    def test_forward_and_backward_targets(self):
        image = bytearray(0x1000)
        image[0x100:0x104] = bl(FLASH + 0x100, FLASH + 0x800)
        image[0x900:0x904] = bl(FLASH + 0x900, FLASH + 0x010)
        calls = call_closure.scan_bl(bytes(image))
        self.assertEqual(calls[FLASH + 0x100], FLASH + 0x800)
        self.assertEqual(calls[FLASH + 0x900], FLASH + 0x010)


class ClosureTests(unittest.TestCase):
    def test_inner_functions_descend_and_boundary_stops(self):
        image = bytearray(0x2000)
        # inner A (0x100) -> inner B (0x200) -> boundary C (0x1800); C -> inner D (0x300) must not be followed
        image[0x100:0x104] = bl(FLASH + 0x100, FLASH + 0x200)
        image[0x200:0x204] = bl(FLASH + 0x200, FLASH + 0x1800)
        image[0x1800:0x1804] = bl(FLASH + 0x1800, FLASH + 0x300)
        with tempfile.TemporaryDirectory() as tmp:
            img = Path(tmp) / "img.bin"; img.write_bytes(bytes(image))
            ents = Path(tmp) / "entries.txt"
            ents.write_text("\n".join(f"{a:08x} bl" for a in (FLASH + 0x100, FLASH + 0x200, FLASH + 0x300, FLASH + 0x1800)) + "\n")
            fns = call_closure.build(str(img), str(ents), [FLASH + 0x100], [(FLASH, FLASH + 0x1000)])
        self.assertEqual(sorted(fns), [FLASH + 0x100, FLASH + 0x200, FLASH + 0x1800])
        self.assertFalse(fns[FLASH + 0x100]["boundary"])
        self.assertTrue(fns[FLASH + 0x1800]["boundary"])
        self.assertEqual(fns[FLASH + 0x1800]["callers"], [FLASH + 0x200])
        self.assertNotIn(FLASH + 0x300, fns)


if __name__ == "__main__":
    unittest.main()
