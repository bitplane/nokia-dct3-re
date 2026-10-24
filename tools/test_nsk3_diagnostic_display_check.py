import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools import nsk3_diagnostic_display_check as checker


class Nsk3DiagnosticDisplayTest(unittest.TestCase):
    def test_own_descriptors_and_key_table_cannot_be_substituted(self):
        image = bytearray(0x100000)
        for address, encoded in checker.DESCRIPTORS:
            values = bytes.fromhex(encoded)
            image[address-0x200000:address-0x200000+len(values)] = values
        image[0xc7a30:0xc7a49] = bytes((62,62,62,62,62,17,25,1,2,3,14,23,4,5,6,15,24,7,8,9,16,26,12,10,11))
        checker.contract(image)
        for address in [x[0] for x in checker.DESCRIPTORS] + [0x2c7a30]:
            damaged = bytearray(image)
            damaged[address-0x200000] ^= 1
            with self.subTest(address=address), self.assertRaises(ValueError):
                checker.contract(damaged)

    def test_complete_bank_hash_is_required_not_a_range_whitelist(self):
        original = b'\xff'*8192
        selected = bytearray(original)
        selected[0x3a9] = 0x3f
        with patch.object(checker, 'SEED_EEPROM', hashlib.sha256(original).hexdigest()), \
                patch.object(checker, 'SELECTED_EEPROM', hashlib.sha256(selected).hexdigest()):
            checker.check_bank(original)
            checker.check_bank(selected, True)
            for offset in (0x100,0x3a9,0x1967,0x1d24):
                damaged = bytearray(selected)
                damaged[offset] ^= 1
                with self.subTest(offset=offset), self.assertRaises(ValueError):
                    checker.check_bank(damaged, True)
            with self.assertRaises(ValueError):
                checker.check_bank(selected[:-1], True)

    def test_battery_acknowledgement_and_all_contacts_are_required(self):
        text = '\n'.join(f'nsk3_diagnostic_key: key={key} down={state} t=42.0 physical_only=1'
                         for key in checker.KEYS for state in ('1','0'))
        checker.check_actions(text)
        for bad in (text.replace('key=up','key=down'), text.replace('down=0','down=1'),
                    text+'\nnsk3_diagnostic_key: malformed', text+'\n'+text):
            with self.assertRaises(ValueError):
                checker.check_actions(bad)
        short = checker.KEYS[:3]+checker.KEYS[4:]
        missing = '\n'.join(f'nsk3_diagnostic_key: key={key} down={state} t=42.0 physical_only=1'
                            for key in short for state in ('1','0'))
        with self.assertRaises(ValueError):
            checker.check_actions(missing)

    def test_native_positive_controls_cannot_be_missing_or_illegal(self):
        text = '\n'.join(f'nsk3_diagnostic_native: phase=cold sample={i} pc=00408d stopped=0 idle=1 observer_writes=0'
                         for i in range(1,6))
        checker.check_native(text,'cold',5)
        for bad in (text.replace('stopped=0','stopped=1'), text.replace('sample=3','sample=2'),
                    text.replace('phase=cold','phase=physical'), text+'\nnsk3_diagnostic_native: malformed'):
            with self.assertRaises(ValueError):
                checker.check_native(bad,'cold',5)

    def test_cold_link_requires_exact_source_and_initial_device_bytes(self):
        with tempfile.TemporaryDirectory() as temporary:
            seed, run = (Path(temporary)/name for name in ('seed','run'))
            storage = seed/'nvram/nsk3r4t'
            storage.mkdir(parents=True)
            run.mkdir()
            for name in ('flash','eeprom','sim_card'):
                (storage/name).write_bytes(name.encode())
                if name != 'flash':
                    (run/f'initial_{name}.bin').write_bytes(name.encode())
            metadata = dict(source=str(seed.resolve()), source_sha256={
                name:hashlib.sha256(name.encode()).hexdigest() for name in ('flash','eeprom','sim_card')},
                copied_directories=['nvram','cfg'], ram_or_save_state_copied=False,
                fdn_fixture=False, fdn_adn_access=False)
            path = run/'cold_input.json'
            path.write_text(json.dumps(metadata))
            checker.check_link(seed,run)
            metadata['ram_or_save_state_copied'] = True
            path.write_text(json.dumps(metadata))
            with self.assertRaises(ValueError):
                checker.check_link(seed,run)
            metadata['ram_or_save_state_copied'] = False
            path.write_text(json.dumps(metadata))
            (run/'initial_eeprom.bin').write_bytes(b'changed')
            with self.assertRaises(ValueError):
                checker.check_link(seed,run)

    def test_input_signature_rejects_model_changes_and_retains_values(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary)/'config.xml'
            path.write_text('<mameconfig><system name="nsk3r4t"><input><port tag=":A" value="0"/></input></system></mameconfig>')
            baseline = checker.input_signature(path)
            path.write_text('<mameconfig><system name="nsk3r4t"><input><port tag=":A" value="1"/></input></system></mameconfig>')
            self.assertNotEqual(baseline,checker.input_signature(path))
            path.write_text('<mameconfig><system name="nsb1r4t"><input/></system></mameconfig>')
            with self.assertRaises(ValueError):
                checker.input_signature(path)


if __name__ == '__main__':
    unittest.main()
