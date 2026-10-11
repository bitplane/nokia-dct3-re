import unittest
from pathlib import Path

from tools.noki6210_selection_result_check import check_fields


class SelectionResultTest(unittest.TestCase):
    def test_rejects_foreign_rom(self):
        with self.assertRaisesRegex(ValueError, 'NPE-3'):
            check_fields(b'foreign image')

    def test_executes_own_decoder(self):
        path = Path(__file__).resolve().parents[1] / 'roms/noki6210/6210_556c.fls'
        if not path.exists():
            self.skipTest('acquired firmware unavailable')
        result = check_fields(path.read_bytes())
        self.assertEqual(result['executed_cases'], 48)
        self.assertEqual(result['record_transition_cases'], 9)
        self.assertEqual(result['request_copy_cases'], 96)
        self.assertEqual(result['record_mode_cases'], 16)
        self.assertEqual(result['phase_modulus'], 5000)
        self.assertEqual(result['frame_modulus'], 51)
        self.assertEqual(result['decoded_prefix_octets'], 8)


if __name__ == '__main__':
    unittest.main()
