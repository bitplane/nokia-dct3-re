"""Exercise isolated acceptance setup without launching MAME."""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET

from tools import run_noki5210_acceptance as runner
from tools.make_sim_card_profile import make_profile


class RunnerTests(unittest.TestCase):
    def test_scenario_commands_and_storage(self):
        expected = {
            'toolkit-interactive': (65, None, None),
            'pin-registration': (45, None, None),
            'late-pin-boundary': (45, '--late-boundary', None),
            'late-pin-registration': (45, '--coherent-late', ()),
            'pin-incoming-call': (45, '--incoming-call', ('2', '32')),
            'pin-incoming-sms': (45, '--incoming-sms', ('4',)),
            'pin-idle-state': (45, '--idle-state', ()),
            'pin-call-state': (55, '--call-state', ('2', '32')),
            'pin-sms-state': (45, '--sms-state', ('4',)),
            'pin-outgoing-call': (60, '--outgoing-call', ('32',)),
            'pin-outgoing-sms': (70, '--outgoing-sms', ()),
            'pin-outgoing-call-state': (60, '--outgoing-call-state', ('32',)),
            'pin-host-outgoing-sms': (70, '--outgoing-sms', ()),
            'pin-host-incoming-sms': (45, '--incoming-sms', ()),
            'pin-host-rejected-sms': (80, '--rejected-sms', ()),
            'pin-host-silent-sms': (155, '--silent-sms', ()),
        }
        self.assertEqual(set(expected), set(runner.SCENARIOS))
        with tempfile.TemporaryDirectory() as temporary:
            for name, (seconds, flag, masks) in expected.items():
                with self.subTest(scenario=name):
                    run = Path(temporary) / name
                    with patch('sys.argv', ['runner', str(run), '--scenario', name]), \
                            patch.object(runner.subprocess, 'run') as execute:
                        runner.main()
                    host = name.startswith('pin-host-')
                    incoming = name == 'pin-host-incoming-sms'
                    rejected = name == 'pin-host-rejected-sms'
                    silent = name == 'pin-host-silent-sms'
                    self.assertEqual(execute.call_count, 3 if host else 2)
                    command = execute.call_args_list[0].args[0]
                    if host:
                        self.assertEqual(Path(command[1]).name,
                                         'run_host_incoming_sms_gate.py' if incoming else 'run_host_sms_gate.py')
                        if not incoming:
                            self.assertEqual(command[command.index('--user-data') + 1], '61')
                            self.assertEqual(command[command.index('--decision') + 1],
                                             'rp_silence' if silent else 'rp_error' if rejected else 'accept')
                        command = command[command.index('--') + 1:]
                    self.assertEqual(command[1:4], ['noki5210', '-bios', '540e'])
                    self.assertEqual(command[command.index('-seconds_to_run') + 1], str(seconds))
                    self.assertEqual(execute.call_args_list[0].kwargs['cwd'], run)
                    self.assertTrue(execute.call_args_list[0].kwargs['check'])
                    script = command[command.index('-autoboot_script') + 1]
                    self.assertTrue(Path(script).is_file())
                    checker = execute.call_args_list[1].args[0]
                    self.assertEqual(checker[2:], [str(run)] + ([flag] if flag else []))
                    if host:
                        host_check = execute.call_args_list[2].args[0]
                        self.assertEqual(Path(host_check[1]).name,
                                         'radio_incoming_host_sms_trace_check.py' if incoming
                                         else 'radio_outgoing_host_sms_trace_check.py')
                        self.assertEqual(host_check[2:], (['--arfcn', '86'] if incoming
                                         else ['--octets', '1', '--outcome',
                                               'rp_silence' if silent else 'rp_error' if rejected else 'rp_ack']) +
                                         [str(run / 'error.log')])
                    config = run / 'cfg/noki5210.cfg'
                    if name == 'toolkit-interactive':
                        source = Path(runner.__file__).resolve().parents[1] / \
                            'fixtures/sim_toolkit_5210_interactive/noki5210.cfg'
                        self.assertEqual(config.read_bytes(), source.read_bytes())
                        self.assertFalse((run / 'nvram/noki5210_3').exists())
                    else:
                        self.assertEqual((run / 'nvram/noki5210_3/sim_card').read_bytes(),
                                         make_profile(pin_enabled=True))
                        self.assertFalse((run / 'nvram/noki5210').exists())
                        if masks is None:
                            self.assertFalse(config.exists())
                        else:
                            ports = ET.parse(config).findall('.//port')
                            self.assertEqual([(p.get('tag'), p.get('mask'), p.get('value'))
                                              for p in ports],
                                             [(':NEIGHBORCFG', '32', '32')] +
                                             [(':NETCFG', m, m) for m in masks] +
                                             ([(':CALLHOST', '1', '1')] if host else []))

    def test_existing_directory_is_rejected_without_launch(self):
        with tempfile.TemporaryDirectory() as run, \
                patch('sys.argv', ['runner', run]), \
                patch.object(runner.subprocess, 'run') as execute:
            with self.assertRaises(SystemExit) as result:
                runner.main()
            self.assertEqual(result.exception.code, 1)
            execute.assert_not_called()

    def test_observation_restricted_to_early_control(self):
        with patch('sys.argv', ['runner', 'unused', '--scenario', 'late-pin-registration',
                                '--observe-readiness']):
            with self.assertRaises(SystemExit) as result:
                runner.main()
            self.assertEqual(result.exception.code, 2)

    def test_early_observation_script(self):
        with tempfile.TemporaryDirectory() as temporary, \
                patch('sys.argv', ['runner', str(Path(temporary) / 'run'),
                                  '--scenario', 'pin-registration', '--observe-readiness']), \
                patch.object(runner.subprocess, 'run') as execute:
            runner.main()
            command = execute.call_args_list[0].args[0]
            self.assertEqual(Path(command[command.index('-autoboot_script') + 1]).name,
                             'noki5210_pin_early_observe.lua')


if __name__ == '__main__':
    unittest.main()
