import hashlib
import unittest
from unittest.mock import patch

from tools import nse5_keypad_check as checker


class Nse5KeypadCheckTest(unittest.TestCase):
    def test_complete_trace(self):
        checker.check_trace("nse5_keypad: PASS matrix_keys=17 scans=85 power_mask=02\n"
                            "nse5_roller: PASS positions=3 probes=18 restored_pairs=9 irq_delivery=unvalidated", 0)

    def test_missing_roller_probe_rejected(self):
        with self.assertRaisesRegex(ValueError, "roller"):
            checker.check_trace("nse5_keypad: PASS matrix_keys=17 scans=85 power_mask=02", 0)

    def test_failed_process_cannot_pass(self):
        with self.assertRaises(ValueError):
            checker.check_trace("nse5_keypad: PASS matrix_keys=17 scans=85 power_mask=02", 1)

    def test_wrong_power_mask_cannot_pass(self):
        with self.assertRaises(ValueError):
            checker.check_trace("nse5_keypad: PASS matrix_keys=17 scans=85 power_mask=04", 0)

    def test_lua_error_cannot_pass(self):
        with self.assertRaises(ValueError):
            checker.check_trace("nse5_keypad: PASS matrix_keys=17 scans=85 power_mask=02\n[LUA ERROR]", 0)

    def test_wrong_image_rejected_before_tables(self):
        with self.assertRaisesRegex(ValueError, "pinned"):
            checker.check_tables(b"")

    def test_tables_checked_in_big_endian_flash_image(self):
        image = bytearray(0x8dd00)
        image[0x8dcd8:0x8dcd8 + 25] = checker.NORMAL
        image[0x8dcf4:0x8dcf4 + 5] = checker.SPECIAL
        with patch.object(checker, "FLASH_SHA1", hashlib.sha1(image).hexdigest()):
            checker.check_tables(image)
        image[0x8dcf5] = 0
        with patch.object(checker, "FLASH_SHA1", hashlib.sha1(image).hexdigest()):
            with self.assertRaisesRegex(ValueError, "special"):
                checker.check_tables(image)


if __name__ == "__main__":
    unittest.main()
