import hashlib
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools import run_noki8210_acceptance as runner


class IsolatedAcceptanceTest(unittest.TestCase):
    def test_dcs_topology_preserves_sms_service(self):
        with tempfile.TemporaryDirectory() as directory:
            config = Path(directory) / 'nsm3hle.cfg'
            config.write_text('<mameconfig version="10"><system name="nsm3hle"><input>'
                              '<port tag=":NETCFG" type="CONFIG" mask="4" value="4"/>'
                              '</input></system></mameconfig>')
            profile = Path(__file__).resolve().parents[1] / 'fixtures/noki8210_dcs1800/nsm3hle.cfg'
            runner.configure_dcs_topology(config, profile)
            ports = runner.ET.parse(config).find("./system/input")
            self.assertEqual(ports.find("./port[@tag=':NETCFG']").get('value'), '4')
            self.assertEqual(ports.find("./port[@tag=':MOBILITYCFG']").get('value'), '10')
            self.assertEqual(ports.find("./port[@tag=':NEIGHBORCFG']").get('value'), '8')
            runner.configure_dcs_topology(config, profile)
            self.assertEqual(len(runner.ET.parse(config).find('./system/input')), 3)

    def test_pin_start_requires_bounded_physical_registration_fixture(self):
        for options in (['--pin-start', '4'],
                        ['--pin-enabled', '--pin-start', 'nan'],
                        ['--pin-enabled', '--pin-start', '2'],
                        ['--pin-enabled', '--pin-start', '21'],
                        ['--pin-enabled', '--pin-start', '4', '--scenario', 'phonebook']):
            with self.subTest(options=options), \
                    patch('sys.argv', ['runner', 'unused'] + options), \
                    patch('sys.stderr', new_callable=io.StringIO), \
                    patch.object(runner, 'prepare_run') as prepare, \
                    self.assertRaises(SystemExit):
                runner.main()
            prepare.assert_not_called()

    def test_pin_measurement_contract_is_band_specific_and_requires_registration(self):
        for dcs, request, carrier in ((False, '57', '0004'), (True, '55', '0337')):
            text = (f'TX packet type={request} payload=4 data=03050000\n'
                    f'RX enqueue type=8b payload=166 data=0010{carrier}00c4\n'
                    '8210_pin_measurement_route: enabled=01 message=001132ac\n'
                    '8210_pin_measurement_completion: message=001132ac\n'
                    '8210_pin_physical: key=Menu\nSIM status ins=20 sw=9000\n'
                    'LAPDm Location Updating Accept acknowledged nr=1')
            with self.subTest(dcs=dcs):
                runner.verify_pin_measurement_registration(text, dcs1800=dcs)
                with self.assertRaises(ValueError):
                    runner.verify_pin_measurement_registration(text, dcs1800=not dcs)
                with self.assertRaises(ValueError):
                    runner.verify_pin_measurement_registration(text.split('LAPDm')[0], dcs1800=dcs)
                with self.assertRaises(ValueError):
                    runner.verify_pin_measurement_registration(
                        text.replace('completion: message=001132ac', 'completion: message=001132ad'),
                        dcs1800=dcs)

    def test_dcs_registration_admitted_with_or_without_pin(self):
        for options in ([], ['--pin-enabled'], ['--pin-enabled', '--pin-start', '7']):
            with patch('sys.argv', ['runner', 'unused', '--dcs1800'] + options), \
                    patch.object(runner.Path, 'read_bytes', return_value=b''), \
                    patch.object(runner, 'prepare_run', side_effect=RuntimeError('admitted')), \
                    self.assertRaisesRegex(RuntimeError, 'admitted'):
                runner.main()

    def test_dcs_does_not_promote_untested_services(self):
        with patch('sys.argv', ['runner', 'unused', '--dcs1800', '--scenario', 'calculator']), \
                patch('sys.stderr', new_callable=io.StringIO), \
                patch.object(runner, 'prepare_run') as prepare, \
                self.assertRaises(SystemExit) as error:
            runner.main()
        self.assertEqual(error.exception.code, 2)
        prepare.assert_not_called()

    def test_dcs_host_outgoing_call_admits_physical_pin_ordering(self):
        for options in ([], ['--pin-enabled', '--pin-start', '7']):
            with self.subTest(options=options), \
                    patch('sys.argv', ['runner', 'unused', '--dcs1800',
                                       '--scenario', 'host-outgoing-call'] + options), \
                    patch.object(runner.Path, 'read_bytes', return_value=b''), \
                    patch.object(runner, 'prepare_run', side_effect=RuntimeError('admitted')), \
                    self.assertRaisesRegex(RuntimeError, 'admitted'):
                runner.main()

    def test_dcs_host_incoming_admits_physical_pin_ordering(self):
        for options in ([], ['--pin-enabled', '--pin-start', '7']):
            with self.subTest(options=options), \
                    patch('sys.argv', ['runner', 'unused', '--dcs1800',
                                       '--scenario', 'host-incoming-call'] + options), \
                    patch.object(runner.Path, 'read_bytes', return_value=b''), \
                    patch.object(runner, 'prepare_run', side_effect=RuntimeError('admitted')), \
                    self.assertRaisesRegex(RuntimeError, 'admitted'):
                runner.main()

    def test_dcs_host_incoming_sms_admits_physical_pin_ordering(self):
        for options in ([], ['--pin-enabled', '--pin-start', '7']):
            with self.subTest(options=options), \
                    patch('sys.argv', ['runner', 'unused', '--dcs1800',
                                       '--scenario', 'host-incoming-sms'] + options), \
                    patch.object(runner.Path, 'read_bytes', return_value=b''), \
                    patch.object(runner, 'prepare_run', side_effect=RuntimeError('admitted')), \
                    self.assertRaisesRegex(RuntimeError, 'admitted'):
                runner.main()

    def test_dcs_outgoing_sms_does_not_claim_pin_coverage(self):
        with patch('sys.argv', ['runner', 'unused', '--dcs1800', '--pin-enabled',
                               '--scenario', 'outgoing-sms']), \
                patch('sys.stderr', new_callable=io.StringIO), \
                patch.object(runner, 'prepare_run') as prepare, \
                self.assertRaises(SystemExit) as error:
            runner.main()
        self.assertEqual(error.exception.code, 2)
        prepare.assert_not_called()

    def test_dcs_host_outgoing_sms_admits_physical_pin_ordering(self):
        for options in ([], ['--pin-enabled', '--pin-start', '7']):
            with self.subTest(options=options), \
                    patch('sys.argv', ['runner', 'unused', '--dcs1800',
                                       '--scenario', 'host-outgoing-sms'] + options), \
                    patch.object(runner.Path, 'read_bytes', return_value=b''), \
                    patch.object(runner, 'prepare_run', side_effect=RuntimeError('admitted')), \
                    self.assertRaisesRegex(RuntimeError, 'admitted'):
                runner.main()

    def test_dcs_host_sms_failure_scenarios_admitted_without_pin(self):
        for scenario in ('host-rejected-sms', 'host-silent-sms'):
            with self.subTest(scenario=scenario), \
                    patch('sys.argv', ['runner', 'unused', '--dcs1800',
                                       '--scenario', scenario]), \
                    patch.object(runner.Path, 'read_bytes', return_value=b''), \
                    patch.object(runner, 'prepare_run', side_effect=RuntimeError('admitted')), \
                    self.assertRaisesRegex(RuntimeError, 'admitted'):
                runner.main()

    def test_dcs_idle_state_admitted_without_pin(self):
        with patch('sys.argv', ['runner', 'unused', '--dcs1800', '--scenario', 'idle-state']), \
                patch.object(runner.Path, 'read_bytes', return_value=b''), \
                patch.object(runner, 'prepare_run', side_effect=RuntimeError('admitted')), \
                self.assertRaisesRegex(RuntimeError, 'admitted'):
            runner.main()

    def test_dcs_incoming_sms_admitted_without_pin(self):
        with patch('sys.argv', ['runner', 'unused', '--dcs1800', '--scenario', 'incoming-sms']), \
                patch.object(runner.Path, 'read_bytes', return_value=b''), \
                patch.object(runner, 'prepare_run', side_effect=RuntimeError('admitted')), \
                self.assertRaisesRegex(RuntimeError, 'admitted'):
            runner.main()

    def test_dcs_sms_state_admitted_without_pin(self):
        with patch('sys.argv', ['runner', 'unused', '--dcs1800', '--scenario', 'sms-state']), \
                patch.object(runner.Path, 'read_bytes', return_value=b''), \
                patch.object(runner, 'prepare_run', side_effect=RuntimeError('admitted')), \
                self.assertRaisesRegex(RuntimeError, 'admitted'):
            runner.main()

    def test_dcs_outgoing_sms_admitted_without_pin(self):
        with patch('sys.argv', ['runner', 'unused', '--dcs1800', '--scenario', 'outgoing-sms']), \
                patch.object(runner.Path, 'read_bytes', return_value=b''), \
                patch.object(runner, 'prepare_run', side_effect=RuntimeError('admitted')), \
                self.assertRaisesRegex(RuntimeError, 'admitted'):
            runner.main()

    def test_dcs_outgoing_call_admitted_without_pin(self):
        with patch('sys.argv', ['runner', 'unused', '--dcs1800', '--scenario', 'outgoing-call']), \
                patch.object(runner.Path, 'read_bytes', return_value=b''), \
                patch.object(runner, 'prepare_run', side_effect=RuntimeError('admitted')), \
                self.assertRaisesRegex(RuntimeError, 'admitted'):
            runner.main()

    def test_dcs_call_state_admitted_without_pin(self):
        with patch('sys.argv', ['runner', 'unused', '--dcs1800', '--scenario', 'call-state']), \
                patch.object(runner.Path, 'read_bytes', return_value=b''), \
                patch.object(runner, 'prepare_run', side_effect=RuntimeError('admitted')), \
                self.assertRaisesRegex(RuntimeError, 'admitted'):
            runner.main()

    def test_dcs_incoming_call_admitted_without_pin(self):
        with patch('sys.argv', ['runner', 'unused', '--dcs1800', '--scenario', 'incoming-call']), \
                patch.object(runner.Path, 'read_bytes', return_value=b''), \
                patch.object(runner, 'prepare_run', side_effect=RuntimeError('admitted')), \
                self.assertRaisesRegex(RuntimeError, 'admitted'):
            runner.main()

    def test_dcs_phonebook_admitted_without_pin(self):
        with patch('sys.argv', ['runner', 'unused', '--dcs1800', '--scenario', 'phonebook']), \
                patch.object(runner.Path, 'read_bytes', return_value=b''), \
                patch.object(runner, 'prepare_run', side_effect=RuntimeError('admitted')), \
                self.assertRaisesRegex(RuntimeError, 'admitted'):
            runner.main()

    def test_dcs_idle_state_does_not_bypass_unresolved_pin(self):
        with patch('sys.argv', ['runner', 'unused', '--dcs1800', '--scenario', 'idle-state', '--pin-enabled']), \
                patch('sys.stderr', new_callable=io.StringIO), \
                patch.object(runner, 'prepare_run') as prepare, \
                self.assertRaises(SystemExit) as error:
            runner.main()
        self.assertEqual(error.exception.code, 2)
        prepare.assert_not_called()

    def test_pin_service_fixtures_are_explicitly_admitted(self):
        for scenario in ('host-incoming-call', 'host-incoming-sms',
                         'host-outgoing-call', 'host-outgoing-sms', 'phonebook',
                         'idle-state', 'call-state', 'sms-state'):
            with self.subTest(scenario=scenario), \
                    patch('sys.argv', ['runner', 'unused', '--scenario', scenario, '--pin-enabled']), \
                    patch.object(runner.Path, 'read_bytes', return_value=b''), \
                    patch.object(runner, 'prepare_run', side_effect=RuntimeError('admitted')), \
                    self.assertRaisesRegex(RuntimeError, 'admitted'):
                runner.main()

    def test_pin_unsupported_fixture_is_rejected_before_preparation(self):
        with patch('sys.argv', ['runner', 'unused', '--scenario', 'calculator', '--pin-enabled']), \
                patch('sys.stderr', new_callable=io.StringIO), \
                patch.object(runner, 'prepare_run') as prepare, \
                self.assertRaises(SystemExit) as error:
            runner.main()
        self.assertEqual(error.exception.code, 2)
        prepare.assert_not_called()

    def test_pin_topology_preserves_host_and_does_not_duplicate_carrier(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'nsm3hle.cfg'
            path.write_text('<mameconfig><system name="nsm3hle"><input>'
                            '<port tag=":CALLHOST" mask="1" value="1"/>'
                            '</input></system></mameconfig>')
            runner.configure_pin_topology(path)
            runner.configure_pin_topology(path)
            ports = runner.ET.parse(path).findall('./system/input/port')
            self.assertEqual(len(ports), 2)
            self.assertEqual(ports[0].get('tag'), ':CALLHOST')
            self.assertEqual(ports[0].get('value'), '1')
            self.assertEqual(ports[1].get('mask'), '256')
            self.assertEqual(ports[1].get('value'), '256')

    def test_pin_topology_rejects_conflicting_cell_without_rewriting(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'nsm3hle.cfg'
            original = '<mameconfig><system name="nsm3hle"><input><port tag=":NEIGHBORCFG" mask="256" value="0"/></input></system></mameconfig>'
            path.write_text(original)
            with self.assertRaisesRegex(ValueError, 'ARFCN4/5'):
                runner.configure_pin_topology(path)
            self.assertEqual(path.read_text(), original)

    def test_host_sms_requires_coherent_registration_not_default_carrier(self):
        text = 'gsm_call_adapter: network registered=1 arfcn=4 t=12'
        with patch.object(runner, 'verify_registration') as registration, \
                patch.object(runner, 'verify_stage') as stage:
            runner.check_host_registration(text, b'card')
            stage.assert_called_once_with(text, runtime=True, selftest=True, base_record=True)
            registration.assert_called_once_with(text, b'card', configured_carrier=True, dcs1800=False)
            for wrong in ('', text.replace('arfcn=4', 'arfcn=1'),
                          text.replace('arfcn=4', 'arfcn=41')):
                with self.subTest(text=wrong), self.assertRaises(ValueError):
                    runner.check_host_registration(wrong, b'card')

    def test_host_dcs_requires_own_carrier_contract(self):
        text = 'gsm_call_adapter: network registered=1 arfcn=823 t=12'
        with patch.object(runner, 'verify_registration') as registration, \
                patch.object(runner, 'verify_stage'):
            runner.check_host_registration(text, b'card', dcs1800=True)
            registration.assert_called_once_with(text, b'card', configured_carrier=False,
                                                dcs1800=True)
            for carrier in (4, 8230):
                with self.subTest(carrier=carrier), self.assertRaises(ValueError):
                    runner.check_host_registration(text.replace('823', str(carrier)),
                                                   b'card', dcs1800=True)

    def test_wrong_product_rejected_before_creating_run(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory) / 'run'
            with self.assertRaisesRegex(ValueError, 'MCU/PPM'):
                runner.prepare_run(run, b'wrong', b'wrong')
            self.assertFalse(run.exists())

    def test_wrong_pmm_rejected_before_creating_run(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory) / 'run'
            with patch.object(runner, 'MCU_SHA1', hashlib.sha1(b'mcu').hexdigest()):
                with self.assertRaisesRegex(ValueError, 'PMM image'):
                    runner.prepare_run(run, b'mcu', b'wrong')
            self.assertFalse(run.exists())

    def test_fresh_storage_and_existing_run_refusal(self):
        mcu, pmm = b'mcu', b'pmm'
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory) / 'run'
            with patch.object(runner, 'MCU_SHA1', hashlib.sha1(mcu).hexdigest()), \
                    patch.object(runner, 'PMM_SHA256', hashlib.sha256(pmm).hexdigest()), \
                    patch.object(runner, 'base_record_fixture', return_value=b'fixture'):
                runner.prepare_run(run, mcu, pmm)
                flash = run / 'nvram/nsm3hle/flash'
                self.assertEqual(flash.read_bytes(), b'mcufixture')
                flash.write_bytes(b'existing state')
                with self.assertRaises(FileExistsError):
                    runner.prepare_run(run, mcu, pmm)
                self.assertEqual(flash.read_bytes(), b'existing state')
                self.assertEqual(pmm, b'pmm')


if __name__ == '__main__':
    unittest.main()
