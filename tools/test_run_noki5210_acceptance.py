"""Exercise isolated acceptance setup without launching MAME."""
from pathlib import Path
import tempfile
import json
import hashlib
import io
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET

from tools import run_noki5210_acceptance as runner
from tools.make_sim_card_profile import make_profile


class RunnerTests(unittest.TestCase):
    def test_preserved_read_source_is_guarded_independently(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / 'source'
            card = source / 'noki5210_3/sim_card'
            card.parent.mkdir(parents=True)
            card.write_bytes(make_profile(True))
            run = Path(temporary) / 'read'
            def execute(command, **kwargs):
                if command[1] == 'noki5210':
                    card.write_bytes(b'changed source')
            with patch('sys.argv', ['runner', str(run), '--scenario', 'pin-phone-memory-edit-read',
                                   '--preserved-nvram', str(source)]), \
                    patch.object(runner.subprocess, 'run', side_effect=execute), \
                    patch('sys.stderr', io.StringIO()) as errors, self.assertRaises(SystemExit) as caught:
                runner.main()
            self.assertEqual(caught.exception.code, 1)
            self.assertIn('changed its source NVRAM', errors.getvalue())

    def test_storage_inventory_covers_all_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'nested').mkdir()
            (root / 'nested/flash').write_bytes(b'flash')
            (root / 'sim_card').write_bytes(b'card')
            self.assertEqual(runner.storage_inventory(root), {
                'nested/flash': hashlib.sha256(b'flash').hexdigest(),
                'sim_card': hashlib.sha256(b'card').hexdigest()})
            with self.assertRaises(ValueError):
                runner.storage_inventory(root / 'missing')

    def test_phone_memory_cold_read_cannot_modify_source(self):
        with tempfile.TemporaryDirectory() as temporary:
            run = Path(temporary) / 'phone-memory'
            def execute(command, **kwargs):
                if '--scenario' in command and command[command.index('--scenario') + 1] == 'pin-phone-memory-read':
                    card = run / 'nvram/noki5210_3/sim_card'
                    card.write_bytes(b'changed source')
            with patch('sys.argv', ['runner', str(run), '--scenario', 'pin-phone-memory']), \
                    patch.object(runner.subprocess, 'run', side_effect=execute), \
                    patch('sys.stderr', io.StringIO()) as errors, self.assertRaises(SystemExit) as caught:
                runner.main()
            self.assertEqual(caught.exception.code, 1)
            self.assertIn('changed its source NVRAM', errors.getvalue())

    def test_phone_memory_read_requires_preserved_storage(self):
        with tempfile.TemporaryDirectory() as temporary:
            run = Path(temporary) / 'missing-source'
            with patch('sys.argv', ['runner', str(run), '--scenario', 'pin-phone-memory-read']), \
                    patch('sys.stderr', io.StringIO()), self.assertRaises(SystemExit) as caught:
                runner.main()
            self.assertEqual(caught.exception.code, 2)
            self.assertFalse(run.exists())

    def test_pin_ussd_selects_authenticated_physical_checker(self):
        with tempfile.TemporaryDirectory() as temporary:
            run = Path(temporary) / 'ussd'
            with patch('sys.argv', ['runner', str(run), '--scenario', 'pin-ussd']), \
                    patch.object(runner.subprocess, 'run') as execute:
                runner.main()
            launch = execute.call_args_list[0].args[0]
            self.assertTrue(any(value.endswith('noki5210_pin_ussd.lua') for value in launch))
            self.assertEqual((run / 'nvram/noki5210_3/sim_card').read_bytes(),
                             make_profile(pin_enabled=True))
            self.assertEqual(Path(execute.call_args_list[1].args[0][1]).name,
                             'noki5210_pin_ussd_check.py')

    def test_sip_restore_has_its_own_bridge_and_recovery_checker(self):
        with tempfile.TemporaryDirectory() as temporary:
            for direction in ('incoming', 'outgoing'):
                run = Path(temporary) / direction
                with patch('sys.argv', ['runner', str(run), '--scenario',
                           f'pin-sip-{direction}-call-state', '--pjsua', 'pjsua']), \
                        patch.object(runner.subprocess, 'run') as execute:
                    runner.main()
                launch = execute.call_args_list[0].args[0]
                self.assertIn('--restore-call' if direction == 'incoming' else '--restore-outgoing', launch)
                self.assertNotIn('-nothrottle', launch)
                checker = execute.call_args_list[1].args[0]
                self.assertEqual(Path(checker[1]).name, 'noki5210_pin_sip_restore_check.py')
                self.assertEqual('--incoming' in checker, direction == 'incoming')

    def test_real_sip_restore_cannot_fall_back_to_mock_host(self):
        with tempfile.TemporaryDirectory() as temporary:
            for direction in ('incoming', 'outgoing'):
                run = Path(temporary) / direction
                with patch('sys.argv', ['runner', str(run), '--scenario',
                           f'pin-sip-{direction}-call-state']):
                    with self.assertRaises(SystemExit) as caught:
                        runner.main()
                    self.assertEqual(caught.exception.code, 2)
                    self.assertFalse(run.exists())

    def test_input_identity_hashes_actual_bytes_and_marks_absence(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'binary'
            path.write_bytes(b'actual executable')
            identity = runner.file_identity(path)
            self.assertEqual(identity['sha256'], hashlib.sha256(path.read_bytes()).hexdigest())
            self.assertEqual(identity['bytes'], len(path.read_bytes()))
            path.unlink()
            self.assertEqual(runner.file_identity(path),
                             {'path': str(path.resolve()), 'present': False})

    def test_sip_requires_host_call_before_creating_artifacts(self):
        with tempfile.TemporaryDirectory() as temporary:
            run = Path(temporary) / 'invalid'
            with patch('sys.argv', ['runner', str(run), '--scenario',
                       'pin-registration', '--pjsua', 'pjsua']):
                with self.assertRaises(SystemExit) as caught:
                    runner.main()
            self.assertEqual(caught.exception.code, 2)
            self.assertFalse(run.exists())

    def test_host_restoration_does_not_silently_promote_sip(self):
        with tempfile.TemporaryDirectory() as temporary:
            for scenario in ('pin-host-outgoing-call-state', 'pin-host-outgoing-media-state',
                             'pin-host-incoming-call-state', 'pin-host-incoming-media-state'):
                run = Path(temporary) / scenario
                with self.subTest(scenario=scenario), patch('sys.argv', [
                        'runner', str(run), '--scenario', scenario, '--pjsua', 'pjsua']):
                    with self.assertRaises(SystemExit) as caught:
                        runner.main()
                    self.assertEqual(caught.exception.code, 2)
                    self.assertFalse(run.exists())

    def test_sip_call_launch_retains_authentication_and_uses_wall_time(self):
        with tempfile.TemporaryDirectory() as temporary:
            for direction in ('incoming', 'outgoing'):
                run = Path(temporary) / direction
                with patch('sys.argv', ['runner', str(run), '--scenario',
                           f'pin-host-{direction}-call', '--pjsua', 'pjsua']), \
                        patch.object(runner.subprocess, 'run') as execute:
                    runner.main()
                launch = execute.call_args_list[0].args[0]
                self.assertEqual(Path(launch[1]).name, 'run_sip_handset_gate.py')
                self.assertEqual(launch[launch.index('--product') + 1], '5210')
                command = launch[launch.index('--') + 1:]
                self.assertNotIn('-nothrottle', command)
                self.assertEqual('--incoming' in launch, direction == 'incoming')
                self.assertEqual((run / 'nvram/noki5210_3/sim_card').read_bytes(),
                                 make_profile(pin_enabled=True))
                manifest = json.loads((run / 'invocation.json').read_text())
                self.assertEqual(manifest['command'], launch)
                card = next(item for item in manifest['inputs']
                            if item['path'].endswith('/sim_card'))
                self.assertEqual(card['sha256'], hashlib.sha256(
                    make_profile(pin_enabled=True)).hexdigest())

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
            'pin-host-outgoing-call': (60, '--outgoing-call', ('32',)),
            'pin-host-outgoing-call-state': (60, '--outgoing-call-state', ('32',)),
            'pin-host-outgoing-media-state': (65, '--outgoing-call-state', ('32',)),
            'pin-host-incoming-call': (60, '--host-incoming-call', ('32',)),
            'pin-host-incoming-call-state': (60, '--call-state', ('32',)),
            'pin-host-incoming-media-state': (65, '--call-state', ('32',)),
            'pin-sms-wait-state': (155, '--silent-sms-state', ()),
            'pin-phonebook': (55, '--phonebook-save', ()),
            'pin-phone-memory': (70, '--save', ()),
            'pin-phone-memory-read': (55, '--read', ()),
            'pin-phone-memory-edit': (65, '--edit', ()),
            'pin-phone-memory-edit-read': (55, '--edit-read', ()),
            'pin-phone-memory-rename': (65, '--rename', ()),
            'pin-phone-memory-rename-read': (55, '--rename-read', ()),
            'pin-phone-memory-add': (70, '--add', ()),
            'pin-phone-memory-multi-read': (65, '--multi-read', ()),
            'pin-phone-memory-erase-first': (65, '--erase-first', ()),
            'pin-phone-memory-erase-cancel': (65, '--erase-cancel', ()),
            'pin-phone-memory-erase': (65, '--erase', ()),
            'pin-phone-memory-empty-read': (55, '--empty-read', ()),
            'pin-ussd': (60, None, ()),
            'pin-phonebook-read': (45, '--phonebook-read', ()),
        }
        self.assertEqual(set(expected) | {'pin-sip-outgoing-call-state', 'pin-sip-incoming-call-state'},
                         set(runner.SCENARIOS))
        cold_followups = {
            'pin-phone-memory': 'pin-phone-memory-read',
            'pin-phone-memory-edit': 'pin-phone-memory-edit-read',
            'pin-phone-memory-rename': 'pin-phone-memory-rename-read',
            'pin-phone-memory-add': 'pin-phone-memory-multi-read',
            'pin-phone-memory-erase-first': 'pin-phone-memory-read',
            'pin-phone-memory-erase-cancel': 'pin-phone-memory-read',
            'pin-phone-memory-erase': 'pin-phone-memory-empty-read',
        }
        for child in cold_followups.values():
            self.assertTrue(runner.SCENARIOS[child].preserved)
            self.assertIsNone(runner.SCENARIOS[child].cold_scenario)
        with tempfile.TemporaryDirectory() as temporary:
            for name, (seconds, flag, masks) in expected.items():
                with self.subTest(scenario=name):
                    run = Path(temporary) / name
                    argv = ['runner', str(run), '--scenario', name]
                    expected_card = make_profile(pin_enabled=True)
                    preserved_scenarios = {'pin-phonebook-read', 'pin-phone-memory-read',
                                           'pin-phone-memory-edit', 'pin-phone-memory-edit-read',
                                           'pin-phone-memory-rename', 'pin-phone-memory-rename-read',
                                           'pin-phone-memory-add', 'pin-phone-memory-multi-read',
                                           'pin-phone-memory-erase-first',
                                           'pin-phone-memory-erase-cancel', 'pin-phone-memory-erase',
                                           'pin-phone-memory-empty-read'}
                    self.assertEqual(runner.SCENARIOS[name].preserved, name in preserved_scenarios)
                    self.assertEqual(runner.SCENARIOS[name].cold_scenario, cold_followups.get(name))
                    if name in preserved_scenarios:
                        preserved = Path(temporary) / ('preserved-' + name)
                        card = preserved / 'noki5210_3/sim_card'
                        card.parent.mkdir(parents=True)
                        expected_card = b'Ada' + expected_card[3:]
                        card.write_bytes(expected_card)
                        argv.extend(['--preserved-nvram', str(preserved)])
                    with patch('sys.argv', argv), \
                            patch.object(runner.subprocess, 'run') as execute, \
                            patch('tools.radio_host_outgoing_connect_check.verify') as host_verify, \
                            patch('tools.radio_host_outgoing_connect_check.verify_restored') as restored_verify, \
                            patch('tools.radio_outgoing_host_media_restore_trace_check.check') as media_verify, \
                            patch('tools.radio_host_incoming_restore_check.verify') as incoming_restore_verify, \
                            patch('tools.radio_host_incoming_restore_check.verify_media') as incoming_media_verify, \
                            patch.object(Path, 'read_text', return_value='host trace'):
                        runner.main()
                    host = name.startswith('pin-host-')
                    incoming = name == 'pin-host-incoming-sms'
                    rejected = name == 'pin-host-rejected-sms'
                    silent = name == 'pin-host-silent-sms'
                    call = name in ('pin-host-outgoing-call', 'pin-host-incoming-call',
                                    'pin-host-outgoing-call-state', 'pin-host-outgoing-media-state')
                    call = call or name in ('pin-host-incoming-call-state', 'pin-host-incoming-media-state')
                    incoming_call = name in ('pin-host-incoming-call', 'pin-host-incoming-call-state',
                                             'pin-host-incoming-media-state')
                    self.assertEqual(execute.call_count, 3 if (host and not call) or name == 'pin-phonebook' or name in cold_followups else 2)
                    if name == 'pin-host-outgoing-media-state':
                        media_verify.assert_called_once_with('host trace', physical_end=True)
                        host_verify.assert_not_called()
                    elif name == 'pin-host-outgoing-call-state':
                        restored_verify.assert_called_once_with('host trace', '5551234')
                        host_verify.assert_not_called()
                    elif call and not incoming_call:
                        host_verify.assert_called_once_with('host trace', '5551234')
                    else:
                        host_verify.assert_not_called()
                    if name != 'pin-host-outgoing-call-state':
                        restored_verify.assert_not_called()
                    command = execute.call_args_list[0].args[0]
                    self.assertEqual('--observe-restore' in command,
                                     name in ('pin-host-outgoing-call-state', 'pin-host-incoming-call-state'))
                    if name == 'pin-host-incoming-call-state':
                        incoming_restore_verify.assert_called_once_with('host trace')
                    else:
                        incoming_restore_verify.assert_not_called()
                    if name == 'pin-host-incoming-media-state':
                        incoming_media_verify.assert_called_once_with('host trace', 'host trace')
                        self.assertIn('--physical-end', command)
                        self.assertIn('--require-restore', command)
                    else:
                        incoming_media_verify.assert_not_called()
                    if host:
                        self.assertEqual(Path(command[1]).name,
                                         'run_host_incoming_call_gate.py' if name == 'pin-host-incoming-media-state' else
                                         'run_host_incoming_signaling_gate.py' if incoming_call else
                                         'run_host_media_restore_gate.py' if name == 'pin-host-outgoing-media-state' else
                                         'run_host_call_adapter_gate.py' if call else
                                         'run_host_incoming_sms_gate.py' if incoming else 'run_host_sms_gate.py')
                        if incoming_call:
                            self.assertEqual(command[command.index('--caller') + 1], '5551234')
                            self.assertEqual(command[command.index('--ready-file') + 1],
                                             str(run / 'snap/5210_pin_host_idle.png'))
                        elif call:
                            self.assertEqual(command[command.index('--number') + 1], '5551234')
                            if name == 'pin-host-outgoing-media-state':
                                self.assertIn('--physical-end', command)
                            else:
                                self.assertEqual(command[command.index('--decision') + 1], 'connect')
                        elif not incoming:
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
                    if name == 'pin-phonebook' or name in cold_followups:
                        cold = execute.call_args_list[2].args[0]
                        self.assertEqual(cold[2:7], [str(run / 'cold'), '--scenario',
                                         cold_followups.get(name, 'pin-phonebook-read'), '--preserved-nvram', str(run / 'nvram')])
                    if name.startswith('pin-phone-memory'):
                        self.assertEqual(Path(checker[1]).name, 'noki5210_phone_memory_check.py')
                    if host and not call:
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
                                         expected_card)
                        self.assertFalse((run / 'nvram/noki5210').exists())
                        if masks is None:
                            self.assertFalse(config.exists())
                        else:
                            ports = ET.parse(config).findall('.//port')
                            self.assertEqual([(p.get('tag'), p.get('mask'), p.get('value'))
                                              for p in ports],
                                             [(':NEIGHBORCFG', '32', '32')] +
                                             [(':NETCFG', m, m) for m in masks] +
                                             ([(':CALLHOST', '1', '1')] if host else []) +
                                             ([(':SMSCFG', '24', '24')] if name == 'pin-sms-wait-state' else []))

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

    def test_preserved_storage_only_allowed_for_cold_phonebook_read(self):
        for argv in (['runner', 'unused', '--scenario', 'pin-phonebook-read'],
                     ['runner', 'unused', '--preserved-nvram', 'source']):
            with self.subTest(argv=argv), patch('sys.argv', argv), \
                    self.assertRaises(SystemExit) as result:
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
