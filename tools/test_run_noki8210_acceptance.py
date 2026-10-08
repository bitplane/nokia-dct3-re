import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools import run_noki8210_acceptance as runner


class IsolatedAcceptanceTest(unittest.TestCase):
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
            registration.assert_called_once_with(text, b'card', configured_carrier=True)
            for wrong in ('', text.replace('arfcn=4', 'arfcn=1'),
                          text.replace('arfcn=4', 'arfcn=41')):
                with self.subTest(text=wrong), self.assertRaises(ValueError):
                    runner.check_host_registration(wrong, b'card')

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
