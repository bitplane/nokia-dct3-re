import unittest
from pathlib import Path
from tools.noki6210_radio_contract import verify


class RadioContractTest(unittest.TestCase):
    def test_rejects_foreign_image(self):
        with self.assertRaisesRegex(ValueError, 'NPE-3'):
            verify(b'foreign firmware')

    def test_own_destination(self):
        path = Path(__file__).resolve().parents[1] / 'roms/noki6210/6210_556c.fls'
        if not path.exists():
            self.skipTest('acquired firmware unavailable')
        result = verify(path.read_bytes())
        self.assertEqual(result['type_8b_destination_task'], 14)
        self.assertEqual(result['traffic_release'], 'runtime physical End observation required')


if __name__ == '__main__':
    unittest.main()
