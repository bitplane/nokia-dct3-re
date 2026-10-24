import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET

from tools.nsb3_acquisition_check import check_traces
from tools import nsb3_nominal_input


class Nsb3AcquisitionTest(unittest.TestCase):
    def test_success_requires_every_counter_step(self):
        def row(result, sample, count):
            return f'nsb3_acquisition: result={result:08x} sample0={sample:04x} sample1={sample:04x} remaining={count:02x} gain=3f800000 offset=00000000 observer_writes=0'
        control = '\n'.join([row(0, 6700, 10)] * 16)
        nominal = 'ccont_adc2_config: raw=22a external_input=1\n' + '\n'.join(row(1, 3628, n) for n in range(10, 0, -1))
        check_traces(control, nominal)
        for bad_control, bad_nominal in [(control, nominal.replace('remaining=03', 'remaining=04')),
                                        (control.replace('1a2c', '0e2c'), nominal),
                                        (control, nominal.replace('result=00000001', 'result=00000000'))]:
            with self.assertRaises(ValueError):
                check_traces(bad_control, bad_nominal)

    def test_preparation_supplies_only_external_config(self):
        image = b'fixture'
        with tempfile.TemporaryDirectory() as directory, patch.object(nsb3_nominal_input, 'IMAGE_SHA256', hashlib.sha256(image).hexdigest()):
            output = Path(directory) / 'new'
            nsb3_nominal_input.prepare(image, output)
            root = ET.parse(output / 'cfg/nsb3r4t.cfg').getroot()
            self.assertEqual(root.find('system/input/port').get('value'), '554')
            self.assertFalse((output / 'nvram').exists())
            with self.assertRaises(FileExistsError):
                nsb3_nominal_input.prepare(image, output)


if __name__ == '__main__':
    unittest.main()
