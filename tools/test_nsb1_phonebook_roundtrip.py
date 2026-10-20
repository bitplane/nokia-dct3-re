import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.nsb1_phonebook_roundtrip import (
    SAVE_KEYS, ERASE_KEYS, check_frame, check_reloaded, check_saved, prepare_reload,
    check_empty_storage, check_erased,
)
from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH


class PhonebookRoundtripTest(unittest.TestCase):
    def storage(self):
        data = bytearray(b'\xff' * CURRENT_NVRAM_LENGTH)
        data[:3] = b'ADA'
        data[18:22] = bytes((3, 0x81, 0x21, 0xf3))
        return bytes(data)

    def save_trace(self):
        return '\n'.join(
            f'nsb1_phonebook_contact: index={index} key={key} down={down} t=30.0 physical_only=1'
            for index, key in enumerate(SAVE_KEYS, 1) for down in (1, 0)) + (
            '\nsim_device: header cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3a\n'
            'sim_device: update fid=6f3a record=1 length=32\n')

    def reload_trace(self):
        return '\n'.join(
            f'nsb1_phonebook_search: index={index} down={down} t=30.0 physical_only=1'
            for index in (1, 2) for down in (1, 0)) + (
            '\nsim_device: header cla=a0 ins=b2 p1=01 p2=04 p3=20 selected=6f3a\n')

    def test_save_requires_contacts_apdu_commit_and_exact_storage(self):
        with patch('tools.nsb1_phonebook_roundtrip.check_frame') as frame:
            check_saved(self.save_trace(), self.storage(), Path('.'))
            frame.assert_called_once()
            for old, new in (('physical_only=1', 'physical_only=0'), ('ins=dc', 'ins=b2'),
                             ('update fid', 'missing fid')):
                with self.subTest(change=old), self.assertRaises(ValueError):
                    check_saved(self.save_trace().replace(old, new), self.storage(), Path('.'))
            with self.assertRaises(ValueError):
                check_saved(self.save_trace(), b'\xff' * CURRENT_NVRAM_LENGTH, Path('.'))

    def test_cold_search_requires_read_without_rewrite(self):
        with patch('tools.nsb1_phonebook_roundtrip.check_frame') as frame:
            check_reloaded(self.reload_trace(), self.storage(), Path('.'))
            frame.assert_called_once()
            for trace in (self.reload_trace().replace('ins=b2', 'ins=f2'),
                          self.reload_trace().replace('index=2', 'index=1'),
                          self.reload_trace() + 'update fid=6f3a record=1',
                          self.reload_trace() + 'ins=dc p1=01 selected=6f3a'):
                with self.subTest(trace=trace), self.assertRaises(ValueError):
                    check_reloaded(trace, self.storage(), Path('.'))

    def test_blank_frame_is_not_contact_presentation(self):
        from PIL import Image
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'blank.png'
            Image.new('RGB', (84, 48), 'white').save(path)
            with self.assertRaisesRegex(ValueError, 'frame mismatch'):
                check_frame(path, '0' * 64)

    def erase_trace(self,cold=False):
        phase,keys = ('cold',('menu','menu')) if cold else ('erase',ERASE_KEYS)
        return '\n'.join(f'nsb1_phonebook_erase: phase={phase} index={index} key={key} down={down} t=30.0 physical_only=1'
            for index,key in enumerate(keys,1) for down in (1,0)) + (
            '\nins=b2 p1=01 p2=04 p3=20 selected=6f3a' if cold else
            '\nins=dc p1=01 p2=04 p3=20 selected=6f3a\nupdate fid=6f3a record=1 length=32\nSIM status ins=dc sw=9000')

    def test_erase_requires_physical_confirmation_success_and_exact_blank_storage(self):
        blank = b'\xff'*CURRENT_NVRAM_LENGTH
        with patch('tools.nsb1_phonebook_roundtrip.check_frame') as frame:
            check_erased(self.erase_trace(),blank,Path('.'))
            self.assertEqual(frame.call_count,2)
            for trace in (self.erase_trace().replace('index=7','index=6'),
                          self.erase_trace().replace('sw=9000','sw=9804'),
                          self.erase_trace()+'\nupdate fid=6f3a record=2 length=32'):
                with self.subTest(trace=trace),self.assertRaises(ValueError):
                    check_erased(trace,blank,Path('.'))
        for card in (self.storage(),blank[:-1]):
            with self.assertRaises(ValueError):
                check_empty_storage(card)

    def test_cold_empty_search_requires_read_and_forbids_write(self):
        blank = b'\xff'*CURRENT_NVRAM_LENGTH
        with patch('tools.nsb1_phonebook_roundtrip.check_frame') as frame:
            check_erased(self.erase_trace(True),blank,Path('.'),cold=True)
            frame.assert_called_once()
            for trace in (self.erase_trace(True).replace('ins=b2','ins=f2'),
                          self.erase_trace(True)+'\nupdate fid=6f3a record=1 length=32'):
                with self.subTest(trace=trace),self.assertRaises(ValueError):
                    check_erased(trace,blank,Path('.'),cold=True)

    def source(self, root):
        source = root / 'save'
        inputs = source / 'inputs/nsb1r4t'
        storage = source / 'nvram/nsb1r4t'
        inputs.mkdir(parents=True)
        storage.mkdir(parents=True)
        qualified = []
        for name, data in (('own.fls', b'own executable'), ('program.bin', b'program'), ('data.bin', b'data')):
            (inputs / name).write_bytes(data)
            qualified.append((name, hashlib.sha1(data).hexdigest()))
        (storage / 'flash').write_bytes(b'own executable')
        (storage / 'eeprom').write_bytes(b'\xff' * 8192)
        (storage / 'sim_card').write_bytes(self.storage())
        (source / 'cfg').mkdir()
        (source / 'cfg/nsb1r4t.cfg').write_text('<mameconfig version="10"/>')
        manifest = {'inputs': [{'source': name, 'sha1': digest} for name, digest in qualified],
                    'synthetic_sim_inserted': True, 'synthetic_retry_initialized': True,
                    'external_eeprom': 'diagnostic checksum/retry control; calibration/identity not validated'}
        (source / 'input_manifest.json').write_text(json.dumps(manifest))
        return source, qualified

    def test_cold_copy_preserves_only_qualified_own_storage(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, inputs = self.source(root)
            target = root / 'reload'
            with patch('tools.nsb1_phonebook_roundtrip.INPUTS', inputs):
                prepare_reload(target, source)
                for name in ('flash', 'eeprom', 'sim_card'):
                    expected = (source / 'nvram/nsb1r4t' / name).read_bytes()
                    self.assertEqual((target / 'nvram/nsb1r4t' / name).read_bytes(), expected)
                    manifest = json.loads((target / 'input_manifest.json').read_text())
                    self.assertEqual(manifest['cold_storage_sha256'][name], hashlib.sha256(expected).hexdigest())
                with self.assertRaisesRegex(ValueError, 'fresh directory'):
                    prepare_reload(target, source)

    def test_changed_source_is_rejected_before_creating_reload(self):
        for relative, replacement in (('nvram/nsb1r4t/flash', b'changed'),
                                      ('nvram/nsb1r4t/eeprom', b'short'),
                                      ('nvram/nsb1r4t/sim_card', b'\xff' * CURRENT_NVRAM_LENGTH),
                                      ('inputs/nsb1r4t/program.bin', b'changed')):
            with self.subTest(relative=relative), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                source, inputs = self.source(root)
                (source / relative).write_bytes(replacement)
                with patch('tools.nsb1_phonebook_roundtrip.INPUTS', inputs), self.assertRaises(ValueError):
                    prepare_reload(root / 'reload', source)
                self.assertFalse((root / 'reload').exists())

    def test_unqualified_storage_provenance_is_refused(self):
        for field, value in (('synthetic_sim_inserted', False), ('synthetic_retry_initialized', False),
                             ('external_eeprom', 'donor storage'), ('inputs', [])):
            with self.subTest(field=field), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                source, inputs = self.source(root)
                path = source / 'input_manifest.json'
                manifest = json.loads(path.read_text())
                manifest[field] = value
                path.write_text(json.dumps(manifest))
                with patch('tools.nsb1_phonebook_roundtrip.INPUTS', inputs), self.assertRaisesRegex(ValueError, 'provenance'):
                    prepare_reload(root / 'reload', source)
                self.assertFalse((root / 'reload').exists())


if __name__ == '__main__':
    unittest.main()
