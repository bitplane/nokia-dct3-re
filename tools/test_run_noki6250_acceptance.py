from pathlib import Path
import tempfile
import unittest
import xml.etree.ElementTree as ET
from tools.run_noki6250_acceptance import apply_coherent_config


class CoherentConfigTest(unittest.TestCase):
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
