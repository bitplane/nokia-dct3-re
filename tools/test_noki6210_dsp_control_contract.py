from pathlib import Path
import unittest
from tools.noki6210_dsp_control_contract import recover, verify_call_field, parse_mode_pairs


class ControlContractTest(unittest.TestCase):
    def test_paired_modes_are_big_endian_and_one_based(self):
        pairs = parse_mode_pairs(bytes.fromhex('070205050702060407020700060309a506030aa406030ba007020d0507020e0407020f00'))
        self.assertEqual(len(pairs), 9)
        self.assertEqual(pairs[0], dict(mode=1, command8=0x0702, command9=0x0505))
        self.assertEqual(pairs[3], dict(mode=4, command8=0x0603, command9=0x09a5))
        self.assertEqual(pairs[-1], dict(mode=9, command8=0x0702, command9=0x0f00))
        for size in (0, 32, 35, 37, 40):
            with self.assertRaises(ValueError):
                parse_mode_pairs(bytes(size))
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
        self.assertEqual(result['paired_mode_publisher'], 0x303c30)
        self.assertEqual(result['paired_mode_table'], 0x262444)
        self.assertEqual(len(result['paired_modes']), 9)
        self.assertFalse(result['paired_mode_semantics_validated'])
        self.assertFalse(result['negative_mode_input_rejected'])
        self.assertEqual(result['paired_mode_message_byte'], 9)
        self.assertEqual(result['paired_mode_direct_bl_candidates'], [0x30566a])
        self.assertEqual(result['paired_mode_command_byte'], 8)
        self.assertEqual(result['paired_mode_command'], 0x80)
        self.assertEqual(result['paired_mode_dispatch_records_checked'], 55)


if __name__ == '__main__':
    unittest.main()
