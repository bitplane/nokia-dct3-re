from pathlib import Path
import unittest
from tools.noki6210_dsp_control_contract import recover


class ControlContractTest(unittest.TestCase):
    def test_unmatched_image_is_rejected(self):
        with self.assertRaises(ValueError):
            recover(bytes(4096))

    def test_acquired_own_contract_when_available(self):
        path = Path(__file__).resolve().parents[1] / 'roms/noki6210/6210_556c.fls'
        if not path.exists():
            self.skipTest('own acquired ROM not available')
        result = recover(path.read_bytes())
        self.assertEqual(result['field_mask'], 0x200)
        self.assertEqual(result['field_selector'], 0x11)
        self.assertFalse(result['speech_semantics_validated'])


if __name__ == '__main__':
    unittest.main()
