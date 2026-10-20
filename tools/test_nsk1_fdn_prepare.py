import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from tools.nsk1_fdn_prepare import prepare
from tools.nsk1_puk_roundtrip import exhaustion_profile
from tools.sim_fdn_profile import extend


class Nsk1FdnPrepareTests(unittest.TestCase):
    def test_explicit_card_extension_preserves_handset_storage(self):
        before = exhaustion_profile()
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'out'
            def copy(run, source):
                storage = run / 'nvram/nsk1r4t'
                storage.mkdir(parents=True)
                (storage / 'sim_card').write_bytes(before)
                (storage / 'eeprom').write_bytes(b'handset-eeprom')
                (storage / 'flash').write_bytes(b'handset-flash')
                (run / 'input_manifest.json').write_text('{}')
            with patch('tools.nsk1_fdn_prepare.check_run', side_effect=[({}, b'wrong'), ({}, before)]), \
                 patch('tools.nsk1_fdn_prepare.copy_persistent', side_effect=copy):
                prepare(Path('qualified-source'), output)
            storage = output / 'nvram/nsk1r4t'
            self.assertEqual((storage / 'sim_card').read_bytes(), extend(before, True))
            self.assertEqual((storage / 'eeprom').read_bytes(), b'handset-eeprom')
            self.assertEqual((storage / 'flash').read_bytes(), b'handset-flash')
            self.assertEqual((output / 'initial_sim_card.bin').read_bytes(), extend(before, True))
            self.assertIs(json.loads((output / 'input_manifest.json').read_text())['card_fixture']['handset_storage_changed'], False)

    def test_already_extended_input_is_rejected_before_copy(self):
        with patch('tools.nsk1_fdn_prepare.check_run', side_effect=[({}, b'wrong'), ({}, extend(exhaustion_profile(), True))]), \
             patch('tools.nsk1_fdn_prepare.copy_persistent') as copy:
            with self.assertRaises(ValueError):
                prepare(Path('source'), Path('out'))
            copy.assert_not_called()


if __name__ == '__main__':
    unittest.main()
