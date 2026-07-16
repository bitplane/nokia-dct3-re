import pathlib
import tempfile
import unittest

from tools.check_lcd_frame import read_pgm, stable_digest


class CheckLcdFrameTest(unittest.TestCase):
    def test_mask_excludes_animated_pixels(self):
        with tempfile.TemporaryDirectory() as directory:
            first = pathlib.Path(directory) / "first.pgm"
            second = pathlib.Path(directory) / "second.pgm"
            first.write_bytes(b"P5\n3 2\n255\n" + bytes([0, 1, 2, 3, 4, 5]))
            second.write_bytes(b"P5\n3 2\n255\n" + bytes([0, 9, 2, 3, 8, 5]))
            width, height, first_pixels = read_pgm(first)
            _, _, second_pixels = read_pgm(second)
            mask = (1, 0, 1, 2)
            self.assertEqual(
                stable_digest(width, height, first_pixels, mask),
                stable_digest(width, height, second_pixels, mask),
            )

    def test_rejects_truncated_frame(self):
        with tempfile.TemporaryDirectory() as directory:
            frame = pathlib.Path(directory) / "bad.pgm"
            frame.write_bytes(b"P5\n2 2\n255\n\x00")
            with self.assertRaisesRegex(ValueError, "expected 4 pixels"):
                read_pgm(frame)


if __name__ == "__main__":
    unittest.main()
