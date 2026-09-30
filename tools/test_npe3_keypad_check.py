import unittest
from unittest.mock import Mock, patch

from tools import npe3_keypad_check as checker


class Npe3KeypadCheckTest(unittest.TestCase):
    def image(self):
        image = bytearray(0x869d9)
        image[0x869b8:0x869b8 + 25] = checker.NORMAL
        image[0x869d4:0x869d4 + 5] = checker.SPECIAL
        return image

    def digest(self):
        return patch.object(checker.hashlib, "sha1", return_value=Mock(
            hexdigest=Mock(return_value=checker.FLASH_SHA1)))

    def test_tables(self):
        with self.digest():
            checker.check_tables(self.image())

    def test_changed_normal(self):
        image = self.image()
        image[0x869b8 + 5] ^= 1
        with self.digest(), self.assertRaisesRegex(ValueError, "normal"):
            checker.check_tables(image)

    def test_changed_power(self):
        image = self.image()
        image[0x869d4 + 4] = 0x5a
        with self.digest(), self.assertRaisesRegex(ValueError, "special"):
            checker.check_tables(image)

    def test_wrong_image(self):
        with self.assertRaisesRegex(ValueError, "pinned"):
            checker.check_tables(self.image())

    def test_conformance_result(self):
        checker.check_trace("npe3_keypad: PASS matrix_keys=20 scans=100 power_mask=10", 0)

    def test_missing_or_failed_result(self):
        for trace, code in [("", 0), ("npe3_keypad: PASS matrix_keys=19 scans=95 power_mask=10", 0),
                            ("npe3_keypad: PASS matrix_keys=20 scans=100 power_mask=10", 1)]:
            with self.assertRaises(ValueError):
                checker.check_trace(trace, code)

    def test_lua_error_after_success_is_not_ignored(self):
        with self.assertRaisesRegex(ValueError, "Lua"):
            checker.check_trace("npe3_keypad: PASS matrix_keys=20 scans=100 power_mask=10\n[LUA ERROR] later error", 0)


if __name__ == "__main__":
    unittest.main()
