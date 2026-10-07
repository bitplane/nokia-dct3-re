import json
import re
from pathlib import Path
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

from tools.run_sip_handset_gate import outgoing_setup_pattern


ROOT = Path(__file__).resolve().parents[1]


class SipProductPreparationTest(unittest.TestCase):
    def test_media_products_explicitly_enable_their_own_host_adapter(self):
        for product in ('3210', '3310', '3330', '3410', '5210'):
            with self.subTest(product=product):
                root = ET.parse(ROOT / 'fixtures' / 'radio_outgoing_host_adapter'
                                / f'noki{product}.cfg').getroot()
                system = root.find(f"system[@name='noki{product}']")
                self.assertIsNotNone(system)
                port = system.find("input/port[@tag=':CALLHOST']")
                self.assertIsNotNone(port)
                self.assertEqual(port.get('value'), '1')
                self.assertEqual(port.get('mask'), '1')

    def test_live_incoming_defaults_keep_answer_and_fresh_storage(self):
        rule = ('sip-config-test: ; @$(info KEYS=$(SIP_3330_INCOMING_KEYS))'
                '$(info NHM2_KEYS=$(SIP_3410_INCOMING_KEYS))'
                '$(info SCRIPT=$(SIP_3330_INCOMING_SCRIPT))'
                '$(info PRESERVE=$(SIP_HANDSET_PRESERVE_NVRAM))true')
        result = subprocess.run(['make', '--no-print-directory', '--eval', rule,
                                 'sip-config-test'], cwd=ROOT,
                                capture_output=True, text=True, check=True)
        self.assertIn('KEYS=1,2,3,4,5,enter,wait500,c,wait500,c,waitalerting,enter', result.stdout)
        self.assertIn('SCRIPT=../mame_nokia_dct3_input_exerciser.lua', result.stdout)
        self.assertIn('NHM2_KEYS=end,waitalerting,send', result.stdout)
        self.assertIn('PRESERVE=0', result.stdout)

    def test_3330_setup_uses_own_captured_encoding(self):
        line = ('GSM service uplink sapi=0 pd=03 message=05 length=18 '
                'data=03450404600200815e0581551532f4150101')
        self.assertIsNotNone(re.search(outgoing_setup_pattern('3330'), line))
        self.assertIsNone(re.search(outgoing_setup_pattern('3310'), line))
        self.assertIsNone(re.search(outgoing_setup_pattern('3210'), line))

    def expand(self, product, bios=''):
        data = json.loads((ROOT / 'gates.json').read_text())
        gate = next(item for item in data['gates']
                    if item['name'] == 'verify-radio-outgoing-call-sip')
        guard = gate['recipe'][1].strip().removesuffix('\\').strip()
        build = gate['recipe'][3].strip().removesuffix('\\').strip()
        # Evaluate the authored Make expressions, without executing recursive builds.
        source = ('DCT3_EEPROM_GUARD = EEPROM_GUARD\nMAKE = builder\nJOBS = 8\n'
                  f'SIP_HANDSET_MACHINE = {product}\nSIP_HANDSET_BIOS = {bios}\n'
                  f'all:\n\t@$(info GUARD={guard})\n\t@$(info BUILD={build})\n\t@true\n')
        with tempfile.TemporaryDirectory() as directory:
            makefile = Path(directory) / 'Makefile'
            makefile.write_text(source)
            result = subprocess.run(['make', '--no-print-directory', '-f', str(makefile)],
                                    capture_output=True, text=True, check=True)
        return result.stdout

    def test_3210_retains_security_fixture_and_restore_guard(self):
        text = self.expand('noki3210')
        self.assertIn('GUARD=EEPROM_GUARD', text)
        self.assertIn('PHONE=noki3210 BIOS=', text)
        self.assertIn('ERASED_IDENTITY_SECURITY_CODE=12345', text)

    def test_3310_does_not_provision_or_restore_3210(self):
        text = self.expand('noki3310', '639')
        self.assertNotIn('EEPROM_GUARD', text)
        self.assertNotIn('ERASED_IDENTITY_SECURITY_CODE', text)
        self.assertIn('PHONE=noki3310 BIOS=639', text)


if __name__ == '__main__':
    unittest.main()
