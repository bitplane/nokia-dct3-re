import json
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from tools.run_noki8210_pin_delay_matrix import check_leg, STARTS


class PinDelayMatrixTests(unittest.TestCase):
    def fixture(self, directory, start):
        (directory / 'acceptance.json').write_text(json.dumps(dict(
            passed=True, machine='nsm3hle', scenario='registration',
            laboratory_carrier=823, pin_start_seconds=start, native_dsp_complete=False)))
        text = ''.join(f'8210_pin_physical: key=Keypad {i + 1} t={start + i}.000000\n' for i in range(4))
        text += f'8210_pin_physical: key=Menu t={start + 4}.000000\n'
        if start != 7:
            text += 'directed timing failure published arfcn=824 mode=4 status=1\n'
        text += f'Location Updating Accept acknowledged nr=1 t={start + 5}.000000\n'
        (directory / 'error.log').write_text(text)

    def test_fixed_delay_domains(self):
        with TemporaryDirectory() as temporary:
            directory = Path(temporary)
            for start in STARTS:
                self.fixture(directory, start)
                result = check_leg(directory, start)
                self.assertEqual(result['timing_failures'], int(start != 7))

    def test_metadata_does_not_replace_physical_evidence(self):
        with TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.fixture(directory, 8)
            path = directory / 'error.log'
            text = path.read_text()
            for bad in (text.replace('t=8.000000', 't=7.000000'),
                        text.replace('key=Keypad 3', 'key=Keypad 2'),
                        text.replace('t=13.000000', 't=11.000000'),
                        text.replace('directed timing failure published', 'ignored')):
                path.write_text(bad)
                with self.assertRaises(ValueError):
                    check_leg(directory, 8)

    def test_wrong_model_and_duplicate_registration_rejected(self):
        with TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.fixture(directory, 20)
            with (directory / 'error.log').open('a') as stream:
                stream.write('Location Updating Accept acknowledged nr=1 t=26.000000\n')
            with self.assertRaises(ValueError):
                check_leg(directory, 20)
            self.fixture(directory, 20)
            path = directory / 'acceptance.json'
            path.write_text(path.read_text().replace('nsm3hle', 'nsm2hle'))
            with self.assertRaises(ValueError):
                check_leg(directory, 20)
