from pathlib import Path
import unittest
from tools.noki6210_dsp_control_contract import recover, verify_call_field


class ControlContractTest(unittest.TestCase):
    def test_physical_call_field_order_and_ownership(self):
        log = ('6210_call_physical: action=send\n'
               'dsp_control_write: data=870b pc=0042727c r4=0000870b r7=00000008\n'
               '6210_call_physical: action=end\n'
               'dsp_control_write: data=850a pc=0042727c r4=0000850a r7=00000008\n')
        result = verify_call_field(log)
        self.assertEqual(result['call_field'], 0x200)
        self.assertFalse(result['pcm_validated'])
        for invalid in (log.replace('data=870b', 'data=850b'),
                        log.replace('r7=00000008', 'r7=00000009'),
                        '\n'.join(reversed(log.splitlines()))):
            with self.assertRaises(ValueError):
                verify_call_field(invalid)

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
        self.assertEqual(len(result['direct_bl_candidates']), 11)
        self.assertFalse(result['indirect_call_coverage'])


if __name__ == '__main__':
    unittest.main()
