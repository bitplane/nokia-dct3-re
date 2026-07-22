import pathlib
import tempfile
import unittest

from tools.check_lcd_frame import crop_digest, read_pgm, stable_digest


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

    def test_crop_hashes_only_selected_pixels(self):
        pixels = bytes([0, 1, 2, 3, 4, 5])
        changed = bytes([9, 1, 2, 8, 4, 5])
        crop = (1, 0, 2, 2)
        self.assertEqual(crop_digest(3, 2, pixels, crop), crop_digest(3, 2, changed, crop))

    def test_rejects_empty_crop(self):
        with self.assertRaisesRegex(ValueError, "positive dimensions"):
            crop_digest(3, 2, bytes(6), (0, 0, 0, 2))


if __name__ == "__main__":
    unittest.main()
