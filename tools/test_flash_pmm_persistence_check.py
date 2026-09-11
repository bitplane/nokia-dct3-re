import unittest

import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from flash_pmm_persistence_check import check


class FlashPmmPersistenceCheckTest(unittest.TestCase):
    def test_accepts_changed_pmm_and_unchanged_firmware(self):
        self.assertEqual([], check(b"FW" + b"\xfe\xff", b"FW", b"\xff\xff"))

    def test_rejects_unchanged_pmm(self):
        self.assertIn(
            "still identical",
            check(b"FW" + b"\xff\xff", b"FW", b"\xff\xff")[0])

    def test_rejects_firmware_change(self):
        self.assertIn(
            "firmware prefix changed",
            check(b"FX" + b"\xfe\xff", b"FW", b"\xff\xff")[0])

    def test_rejects_wrong_size(self):
        self.assertIn("size", check(b"FW", b"FW", b"\xff")[0])


if __name__ == "__main__":
    unittest.main()
