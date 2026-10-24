from pathlib import Path
import unittest

from tools.noki6210_service_catalogue import extract


class ServiceCatalogueTest(unittest.TestCase):
    def test_unrecognized_image_rejected(self):
        with self.assertRaisesRegex(ValueError, 'hash mismatch'):
            extract(b'not own NPE-3 MCU')

    def test_own_image_when_available(self):
        image = Path(__file__).resolve().parents[1] / 'roms/noki6210/6210_556c.fls'
        if not image.exists():
            self.skipTest('own MCU unavailable')
        result = extract(image.read_bytes())
        self.assertEqual((result['extent'], result['non_null']), (103, 94))
        self.assertEqual(result['slots'][0]['pointer'], '004b0477')
        self.assertEqual(result['slots'][1]['pointer'], '004dbfeb')
        self.assertEqual(result['slots'][2]['pointer'], '004dc061')
