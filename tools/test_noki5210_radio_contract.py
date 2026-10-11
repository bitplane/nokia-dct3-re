import unittest
from pathlib import Path
from tools.noki5210_radio_contract import verify


class RadioContractTest(unittest.TestCase):
    def test_foreign_image(self):
        with self.assertRaisesRegex(ValueError, 'NSM-5'):
            verify(b'foreign firmware')

    def test_own_receive_grammar(self):
        path = Path(__file__).resolve().parents[1] / 'roms/noki5210/5210_5.40_ppm_e.fls'
        if not path.exists():
            self.skipTest('acquired firmware unavailable')
        result = verify(path.read_bytes())
        self.assertEqual(result['type_8b_destination_task'], 12)
        self.assertEqual(result['records'], 40)
        self.assertEqual(result['signed_rssi_message_offset'], 9)


if __name__ == '__main__':
    unittest.main()
