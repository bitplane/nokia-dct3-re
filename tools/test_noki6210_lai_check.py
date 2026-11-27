from pathlib import Path
import unittest

from tools.noki6210_lai_check import check_lai


ROM = Path(__file__).resolve().parents[1] / 'roms/noki6210/6210_556c.fls'


@unittest.skipUnless(ROM.exists(), 'requires the 6210 v5.56 ROM')
class LaiParserTest(unittest.TestCase):
    def test_own_parser_requires_two_digit_mnc_filler(self):
        image = ROM.read_bytes()
        self.assertEqual(check_lai(image), {
            'cases': 112, 'accepted_mnc_filler': 15, 'minimum_length': 5})

    def test_foreign_image_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'NPE-3'):
            check_lai(bytes(4096))
