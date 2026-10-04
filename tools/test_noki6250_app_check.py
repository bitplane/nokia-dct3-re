import unittest
from hashlib import sha256
from unittest.mock import patch
from tools.noki6250_app_check import verify


class AppCheckTests(unittest.TestCase):
    def log(self):
        return "\n".join(f"6250_app_input: step={n} pressed={n % 2}" for n in range(1, 29))

    def test_physical_sequence_and_frame(self):
        pixels = bytes(96 * 60)
        with patch("tools.noki6250_app_check.RESULT", sha256(pixels).hexdigest()):
            verify(self.log(), pixels, (96, 60))

    def test_missing_equals_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "input 27"):
            verify(self.log().replace("step=27", "step=99"), bytes(96 * 60), (96, 60))

    def test_wrong_result_frame(self):
        with self.assertRaisesRegex(ValueError, "1\\+2=3"):
            verify(self.log(), bytes(96 * 60), (96, 60))


if __name__ == "__main__":
    unittest.main()
