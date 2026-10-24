import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools import nsb3_phonebook_check as checker
from tools.nsk3_phonebook_check import expected_record
from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH


def trace(keys):
    lines = ['nsb3_phonecode_matched_branch: retry=00 observer_writes=0',
             'nsb3_phonecode_menu: down=1 physical_only=1',
             'nsb3_phonecode_menu: down=0 physical_only=1']
    lines += [f'name=Keypad {digit} down={state}' for digit in '12345' for state in (1, 0)]
    lines += [f'nsb3_phonebook_key: key={key} down={state} t=40.0 physical_only=1'
              for key in keys for state in (1, 0)]
    return '\n'.join(lines)


class Nsb3PhonebookTest(unittest.TestCase):
    def test_cold_source_cycle_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            (source / 'cold_source.json').write_text(json.dumps(dict(
                source=str(source), copied_guest_ram_or_save_state=False)))
            with self.assertRaisesRegex(ValueError, 'cyclic'):
                checker.fixture_origin(source)

    def test_contacts_require_exact_phase_and_matched_code(self):
        for readback, keys in ((False, checker.SAVE_KEYS), (True, checker.READ_KEYS)):
            text = trace(keys)
            checker.check_contacts(text, readback)
            for bad in (text.replace('matched_branch', 'unmatched'),
                        text.replace('physical_only=1', 'physical_only=0'), trace(keys[:-1])):
                with self.assertRaises(ValueError):
                    checker.check_contacts(bad, readback)

    def test_wire_decodes_update_body_not_embedded_opcode_bytes(self):
        tx = bytes.fromhex('ff00ffa0a40000026f3aa0dc010420') + expected_record()
        text = '\n'.join(f'nsb3_sim_wire: direction=TX data={byte:02x} t=63.0 observer_writes=0' for byte in tx)
        self.assertEqual(checker.wire(text)[-1], (0xdc, 1, 4, 32, expected_record(), 0x6f3a))
        with self.assertRaises(ValueError):
            checker.wire(text.rsplit('\n', 1)[0])
        with self.assertRaises(ValueError):
            checker.wire('')

    def test_cold_preparation_copies_only_own_nonvolatile_state(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, cold = root / 'source', root / 'cold'
            storage = source / 'nvram/nsb3r4t'
            storage.mkdir(parents=True)
            (source / 'cfg').mkdir()
            (source / 'cfg/nsb3r4t.cfg').write_text('<mameconfig/>')
            flash = b'own-test-executable'
            (storage / 'flash').write_bytes(flash)
            (storage / 'eeprom').write_bytes(bytes(8192))
            (storage / 'sim_card').write_bytes(expected_record() + b'\xff' * (CURRENT_NVRAM_LENGTH - 32))
            (source / 'input.json').write_text(json.dumps(dict(insert_sim=True, fresh_phone_retry_initialized=True,
                                                              image_sha256=hashlib.sha256(flash).hexdigest())))
            (source / 'state.sta').write_bytes(b'not permitted')
            with patch.object(checker, 'IMAGE_SHA256', hashlib.sha256(flash).hexdigest()):
                checker.prepare_reload(source, cold)
                self.assertFalse((cold / 'state.sta').exists())
                for name in ('flash', 'eeprom', 'sim_card'):
                    self.assertEqual((cold / 'nvram/nsb3r4t' / name).read_bytes(), (storage / name).read_bytes())
                with self.assertRaises(FileExistsError):
                    checker.prepare_reload(source, cold)
                second = root / 'second'
                checker.prepare_reload(cold, second)
                self.assertEqual(json.loads((second / 'cold_source.json').read_text())['fixture_origin'], str(source))
            with self.assertRaises(ValueError):
                checker.prepare_reload(source, root / 'bad')
            self.assertFalse((root / 'bad').exists())


if __name__ == '__main__':
    unittest.main()
