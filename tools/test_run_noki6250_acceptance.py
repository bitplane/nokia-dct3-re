from pathlib import Path
import io
import tempfile
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET
from tools.run_noki6250_acceptance import apply_coherent_config, prepare_run, prerequisite_trace, main, check_ussd


class CoherentConfigTest(unittest.TestCase):
    def test_ussd_uses_own_reviewed_idle_frame(self):
        with patch('tools.noki8210_supplementary_check.verify_transaction') as verify:
            check_ussd('trace', Path('frames'))
        args, kwargs = verify.call_args
        self.assertEqual(args[3][-1], 'Send')
        self.assertEqual(args[6], '7c541cfc93c2da8e854421941df0ac81755b73f47c3af98f2f6a40efac181b0b')
        self.assertEqual(kwargs, {'product': '6250', 'geometry': (96, 60)})

    def test_saved_prerequisites_exclude_post_load_events(self):
        text = 'before\n6250_state: event=saved pc=1\nafter\n'
        for scenario in ('idle-state', 'call-state', 'sms-state'):
            self.assertEqual(prerequisite_trace(text, scenario), 'before\n')
            with self.assertRaisesRegex(ValueError, 'save boundary'):
                prerequisite_trace('after only', scenario)
        self.assertEqual(prerequisite_trace(text, 'host-incoming-call'), text)

    def test_pin_services_are_admitted_before_preparation(self):
        for scenario in ('host-incoming-call', 'host-incoming-sms', 'host-outgoing-call', 'host-outgoing-sms', 'host-rejected-sms', 'host-silent-sms', 'phonebook', 'idle-state', 'call-state', 'sms-state'):
            with patch('sys.argv', ['runner', 'unused', '--scenario', scenario,
                                   '--coherent-cell', '--pin-enabled', '--mame', '/nonexistent/6250-mame']), \
                    patch('sys.stderr', new_callable=io.StringIO) as errors, \
                    patch('tools.run_noki6250_acceptance.prepare_run') as prepare:
                with self.assertRaises(SystemExit) as result:
                    main()
                self.assertEqual(result.exception.code, 1)
                self.assertIn('missing MAME executable', errors.getvalue())
                prepare.assert_not_called()

    def test_pin_scope_requires_explicit_coherent_host_service(self):
        for options in (['--scenario', 'host-incoming-call'],
                        ['--scenario', 'host-incoming-sms-text', '--coherent-cell'],
                        ['--scenario', 'idle-state']):
            with patch('sys.argv', ['runner', 'unused', '--pin-enabled'] + options), \
                    patch('sys.stderr', new_callable=io.StringIO), \
                    patch('tools.run_noki6250_acceptance.prepare_run') as prepare:
                with self.assertRaises(SystemExit) as result:
                    main()
                self.assertEqual(result.exception.code, 2)
                prepare.assert_not_called()

    def test_coherent_idle_is_admitted_before_preparation(self):
        with patch('sys.argv', ['runner', 'unused', '--scenario', 'idle-state',
                               '--coherent-cell', '--mame', '/nonexistent/6250-mame']), \
                patch('sys.stderr', new_callable=io.StringIO) as errors, \
                patch('tools.run_noki6250_acceptance.prepare_run') as prepare:
            with self.assertRaises(SystemExit) as result:
                main()
            self.assertEqual(result.exception.code, 1)
            self.assertIn('missing MAME executable', errors.getvalue())
            prepare.assert_not_called()

    def test_coherent_active_state_is_admitted_before_preparation(self):
        for scenario in ('call-state', 'sms-state'):
            with patch('sys.argv', ['runner', 'unused', '--scenario', scenario, '--coherent-cell', '--mame', '/nonexistent/6250-mame']), \
                    patch('sys.stderr', new_callable=io.StringIO), \
                    patch('tools.run_noki6250_acceptance.prepare_run') as prepare:
                with self.assertRaises(SystemExit) as result:
                    main()
                self.assertEqual(result.exception.code, 1)
                prepare.assert_not_called()

    def test_shared_preparation_keeps_own_comparison_and_audit_labels(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'roms/noki6250').mkdir(parents=True)
            (root / 'roms/noki3210').mkdir()
            (root / 'roms/noki6250/6250-503mcuppmc.fls').write_bytes(b'own MCU')
            source = root / 'roms/noki6250/6250 virgin eeprom 005fa000.fls'
            source.write_bytes(b'own PMM')
            for name in ('dsp_prom', 'dsp_drom', 'dsp_pdrom'):
                (root / 'roms/noki3210' / name).write_bytes(name.encode())
            run = root / 'fresh'
            with patch('tools.noki6250_accessory_contract.verify', return_value={'own': True}) as contract, \
                    patch('tools.run_noki6250_acceptance.initial_record_fixture', return_value=b'comparison') as fixture:
                accessory, audit = prepare_run(run, root)
                contract.assert_called_once_with(b'own MCU')
                fixture.assert_called_once_with(b'own PMM')
                self.assertEqual(accessory, {'own': True})
                self.assertEqual((run / 'roms/noki6250' / source.name).read_bytes(), b'comparison')
                self.assertTrue(all(member['native_6250_evidence'] is False for member in audit))
                self.assertEqual(source.read_bytes(), b'own PMM')
                with self.assertRaises(FileExistsError):
                    prepare_run(run, root)

    def fixture(self):
        return Path(__file__).resolve().parents[1] / 'fixtures/noki6250_host_gsm900/nhm3hle.cfg'

    def test_reply_target_survives_and_overlay_is_idempotent(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'nhm3hle.cfg'
            path.write_text('<mameconfig><system name="nhm3hle"><input>'
                            '<port tag=":NETCFG" mask="4" value="4"/>'
                            '<port tag=":CALLHOST" mask="1" value="0"/>'
                            '</input></system></mameconfig>')
            for _ in range(2):
                apply_coherent_config(path, self.fixture())
            inputs = ET.parse(path).getroot().find('system/input')
            self.assertEqual(len(inputs), 3)
            self.assertEqual({port.get('tag'): port.get('value') for port in inputs},
                             {':NETCFG': '4', ':CALLHOST': '1', ':NEIGHBORCFG': '512'})

    def test_fresh_incoming_does_not_acquire_automatic_sms_fixture(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'nhm3hle.cfg'
            apply_coherent_config(path, self.fixture())
            inputs = ET.parse(path).getroot().find('system/input')
            self.assertEqual({port.get('tag') for port in inputs}, {':CALLHOST', ':NEIGHBORCFG'})

    def test_other_product_is_rejected_without_rewriting(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'other.cfg'
            original = '<mameconfig><system name="nsm3hle"><input/></system></mameconfig>'
            path.write_text(original)
            with self.assertRaises(ValueError):
                apply_coherent_config(path, self.fixture())
            self.assertEqual(path.read_text(), original)


if __name__ == '__main__':
    unittest.main()
