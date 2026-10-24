from pathlib import Path
import io
import unittest
from unittest.mock import patch
from tools.noki6210_dsp_control_contract import recover, verify_call_field, parse_mode_pairs, verify_mode_observation


class ControlContractTest(unittest.TestCase):
    def test_observer_cli_is_restricted_before_launch(self):
        from tools.run_noki6210_acceptance import main
        for options in ([], ['--scenario', 'outgoing-call'],
                        ['--scenario', 'outgoing-call', '--foreign-cell']):
            with self.subTest(options=options), \
                    patch('sys.argv', ['runner', 'unused', '--trace-dsp-control'] + options), \
                    patch('sys.stderr', new_callable=io.StringIO), \
                    self.assertRaises(SystemExit) as error:
                main()
            self.assertEqual(error.exception.code, 2)

    def test_mode_probe_needs_positive_compiler_and_physical_controls(self):
        log = ('6210_control_compiler: selector=08 value=0102 publish=1 lr=00420c0f\n'
               '6210_call_physical: action=send\n'
               '6210_control_compiler: selector=08 value=070b publish=1 lr=00420c0f\n'
               'dsp_control_write: data=870b pc=0042727c r4=0000870b r7=00000008\n'
               '6210_call_physical: action=end\n'
               '6210_control_compiler: selector=08 value=050a publish=1 lr=00420c0f\n'
               'dsp_control_write: data=850a pc=0042727c r4=0000850a r7=00000008\n')
        result = verify_mode_observation(log)
        self.assertEqual(result['normal_publish_call'], 0x420c0a)
        self.assertFalse(result['pcm_validated'])
        for bad in (log.replace('value=070b', 'value=060b'),
                    log.replace('00420c0f', '00303c81'),
                    log.replace('6210_control_compiler:', 'missing:'),
                    log + '6210_paired_mode: value=01\n',
                    log + '6210_service_command: class=40 command=80 argument=01\n'):
            with self.subTest(bad=bad[-70:]), self.assertRaises(ValueError):
                verify_mode_observation(bad)
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
        self.assertEqual(result['paired_mode_message_class'], 0x40)
        self.assertEqual(result['paired_mode_consumer_direct_bl_candidates'], [0x305ab8])
        self.assertTrue(result['paired_mode_reply_echoes_mode'])
        self.assertEqual(result['normal_parameter_publish_call'], 0x420c0a)
        self.assertEqual(result['normal_parameter_selectors'],
                         [8, 9, 0x1b, 0x25, 0x20, 0x21, 0x22, 0x23, 0x24, 0x28, 0x2d])


if __name__ == '__main__':
    unittest.main()
