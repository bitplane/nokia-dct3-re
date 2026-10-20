import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.nsk1_phonebook_roundtrip import check_saved,check_reloaded,check_contacts,prepare_reload,check_empty,SAVE_KEYS,SEARCH_KEYS,ERASE_KEYS
from tools.nsk1_rom4_compare import INPUTS


class Nsk1PhonebookTest(unittest.TestCase):
    def test_erase_and_cold_empty_reject_storage_or_transport_changes(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            self.setup_storage(run)
            before = self.card()
            empty = b'\xff'*1600+before[1600:]
            card_path = run/'nvram/nsk1r4t/sim_card'
            card_path.write_bytes(empty)
            erase = (self.contacts('nsk1_phonebook_erase',ERASE_KEYS)
                     +'\nins=dc p1=01 p2=04 p3=20 selected=6f3a'
                     +'\nupdate fid=6f3a record=1 length=32\nSIM status ins=dc sw=9000')
            cold = self.contacts('nsk1_phonebook_empty',SEARCH_KEYS)+'\nins=b2 p1=01 p2=04 p3=20 selected=6f3a'
            with patch('tools.nsk1_phonebook_roundtrip.check_security'), \
                    patch('tools.nsk1_phonebook_roundtrip.frame_check'):
                with patch('tools.nsk1_phonebook_roundtrip.check_phase',return_value=({},erase)):
                    self.assertTrue(check_empty(run,bytes(2048),before)[0]['physical_phonebook_erase'])
                    card_path.write_bytes(before)
                    with self.assertRaises(ValueError):
                        check_empty(run,bytes(2048),before)
                    card_path.write_bytes(empty[:-1]+b'\0')
                    with self.assertRaises(ValueError):
                        check_empty(run,bytes(2048),before)
                    card_path.write_bytes(empty)
                with patch('tools.nsk1_phonebook_roundtrip.check_phase',return_value=({},cold)):
                    self.assertTrue(check_empty(run,bytes(2048),empty,True)[0]['physical_cold_empty_search'])
                for invalid in (erase+'\nupdate fid=6f3a record=1 length=32',erase.replace('sw=9000','sw=9404')):
                    with patch('tools.nsk1_phonebook_roundtrip.check_phase',return_value=({},invalid)):
                        with self.assertRaises(ValueError):
                            check_empty(run,bytes(2048),before)
                for invalid in (cold.replace('ins=b2','ins=c0'),cold+'\nupdate fid=6f3a'):
                    with patch('tools.nsk1_phonebook_roundtrip.check_phase',return_value=({},invalid)):
                        with self.assertRaises(ValueError):
                            check_empty(run,bytes(2048),empty,True)

    def card(self):
        data = bytearray([255])*3570
        data[:3] = b'ADA'
        data[18:22] = bytes.fromhex('038121f3')
        return bytes(data)

    def contacts(self,marker,keys):
        return '\n'.join(f'{marker}: index={index} key={key} down={down} t=30.0 physical_only=1'
                         for index,key in enumerate(keys,1) for down in (1,0))

    def setup_storage(self,run):
        storage = run/'nvram/nsk1r4t'
        storage.mkdir(parents=True)
        (storage/'sim_card').write_bytes(self.card())
        (storage/'eeprom').write_bytes(bytes(2048))
        (run/'initial_eeprom.bin').write_bytes(bytes(2048))

    def test_serial_save_and_cold_preservation_checks(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            self.setup_storage(run)
            saved = (self.contacts('nsk1_phonebook_contact',SAVE_KEYS)
                     +'\nsim_device: header ins=dc p1=01 p2=04 p3=20 selected=6f3a'
                     +'\nsim_device: update fid=6f3a record=1 length=32\nSIM status ins=dc sw=9000')
            cold = self.contacts('nsk1_phonebook_search',SEARCH_KEYS)+'\nins=b2 p1=01 p2=04 p3=20 selected=6f3a'
            with patch('tools.nsk1_phonebook_roundtrip.check_phase',return_value=({},saved)), \
                    patch('tools.nsk1_phonebook_roundtrip.check_security') as security, \
                    patch('tools.nsk1_phonebook_roundtrip.frame_check'):
                summary,card = check_saved(run)
                self.assertTrue(summary['physical_phonebook_save'])
                security.assert_called_once()
                with patch('tools.nsk1_phonebook_roundtrip.check_phase',return_value=({},saved+'\nupdate fid=6f3a record=1 length=32')):
                    with self.assertRaises(ValueError):
                        check_saved(run)
                with patch('tools.nsk1_phonebook_roundtrip.check_phase',return_value=({},cold)):
                    self.assertTrue(check_reloaded(run,bytes(2048),card)['physical_cold_contact_search'])
                    with self.assertRaises(ValueError):
                        check_reloaded(run,bytes(2048),card[:-1])
                with patch('tools.nsk1_phonebook_roundtrip.check_phase',return_value=({},cold+'\nupdate fid=6f3a')):
                    with self.assertRaises(ValueError):
                        check_reloaded(run,bytes(2048),card)

    def test_contacts_and_unqualified_source_fail_closed(self):
        trace = self.contacts('nsk1_phonebook_search',SEARCH_KEYS)
        check_contacts(trace,'nsk1_phonebook_search',SEARCH_KEYS)
        for changed in (trace.replace('key=two','key=menu'),trace+'\nnsk1_phonebook_search: malformed'):
            with self.assertRaises(ValueError):
                check_contacts(changed,'nsk1_phonebook_search',SEARCH_KEYS)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root/'source'
            self.setup_storage(source)
            (source/'inputs').mkdir()
            (source/'cfg').mkdir()
            manifest = {'inputs':[{'source':name,'sha1':digest} for name,digest in INPUTS],
                        'adc2_source':554,'inserted_synthetic_sim':True,'peer_completions':False,'phone_retry':{'after':0}}
            (source/'input_manifest.json').write_text(json.dumps(manifest))
            with patch('tools.nsk1_phonebook_roundtrip.check_saved') as saved:
                initial = prepare_reload(root/'cold',source)
                self.assertEqual(bytes(2048),initial)
                saved.assert_called_once_with(source)
                provenance = json.loads((root/'cold/input_manifest.json').read_text())
                self.assertFalse(provenance['ram_or_save_state_copied'])
                self.assertFalse((root/'cold/snap').exists())
                with self.assertRaisesRegex(ValueError,'fresh'):
                    prepare_reload(root/'cold',source)
                manifest['peer_completions'] = True
                (source/'input_manifest.json').write_text(json.dumps(manifest))
                with self.assertRaises(ValueError):
                    prepare_reload(root/'invalid',source)
                self.assertFalse((root/'invalid').exists())


if __name__ == '__main__':
    unittest.main()
