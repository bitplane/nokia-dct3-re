from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.nsk1_sms_draft_roundtrip import check_card,check_run,check_deleted,DRAFT,SMS_OFFSET,RECORD_SIZE
from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH


class Nsk1SmsTest(unittest.TestCase):
    def test_delete_preserves_payload_and_cold_search_never_writes(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            path = run/'nvram/nsk1r4t/sim_card'
            path.parent.mkdir(parents=True)
            (run/'initial_eeprom.bin').write_bytes(bytes(2048))
            saved = self.card()
            deleted = bytearray(saved)
            deleted[SMS_OFFSET] = 0
            path.write_bytes(deleted)
            update = 'ins=dc p1=01 p2=04 p3=b0 selected=6f3c\nupdate fid=6f3c record=1 length=176\nSIM status ins=dc sw=9000'
            read = 'ins=b2 p1=01 p2=04 p3=b0 selected=6f3c'
            with patch('tools.nsk1_sms_draft_roundtrip.check_phone_code'), \
                    patch('tools.nsk1_sms_draft_roundtrip.check_contacts'), \
                    patch('tools.nsk1_sms_draft_roundtrip.frame_check'):
                for cold,previous,trace in ((False,saved,update),(True,deleted,read)):
                    with patch('tools.nsk1_sms_draft_roundtrip.check_phase',return_value=({},trace)):
                        self.assertTrue(check_deleted(run,previous,cold)[0])
                        changed = bytearray(deleted)
                        changed[SMS_OFFSET+12] ^= 1
                        path.write_bytes(changed)
                        with self.assertRaises(ValueError):
                            check_deleted(run,previous,cold)
                        path.write_bytes(deleted)
                for trace,cold,previous in ((update.replace('sw=9000','sw=9404'),False,saved),
                                            (update+'\nupdate fid=6f3c record=1 length=176',False,saved),
                                            (read+'\nupdate fid=6f3c',True,deleted)):
                    with patch('tools.nsk1_sms_draft_roundtrip.check_phase',return_value=({},trace)):
                        with self.assertRaises(ValueError):
                            check_deleted(run,previous,cold)

    def card(self):
        card = bytearray(b'\xff'*CURRENT_NVRAM_LENGTH)
        card[SMS_OFFSET:SMS_OFFSET+RECORD_SIZE] = DRAFT
        card[SMS_OFFSET+RECORD_SIZE:SMS_OFFSET+10*RECORD_SIZE] = (b'\0'+b'\xff'*175)*9
        return bytes(card)

    def test_exact_draft_and_unused_records(self):
        card = self.card()
        check_card(card)
        for offset in (0,SMS_OFFSET,SMS_OFFSET+12,SMS_OFFSET+RECORD_SIZE,SMS_OFFSET+9*RECORD_SIZE):
            mutated = bytearray(card)
            mutated[offset] ^= 1
            with self.assertRaises(ValueError):
                check_card(mutated)
        with self.assertRaises(ValueError):
            check_card(card[:-1])

    def test_transport_and_cold_whole_card_preservation(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            storage = run/'nvram/nsk1r4t'
            storage.mkdir(parents=True)
            (storage/'sim_card').write_bytes(self.card())
            (run/'initial_eeprom.bin').write_bytes(bytes(2048))
            saved = 'ins=dc p1=01 p2=04 p3=b0 selected=6f3c\nupdate fid=6f3c record=1 length=176\nSIM status ins=dc sw=9000'
            cold = 'ins=b2 p1=01 p2=04 p3=b0 selected=6f3c'
            with patch('tools.nsk1_sms_draft_roundtrip.check_phone_code'), \
                    patch('tools.nsk1_sms_draft_roundtrip.check_contacts'), \
                    patch('tools.nsk1_sms_draft_roundtrip.frame_check'):
                for trace in (saved,cold):
                    reading = trace == cold
                    with patch('tools.nsk1_sms_draft_roundtrip.check_phase',return_value=({},trace)):
                        self.assertTrue(check_run(run,reading,self.card())[0])
                        if reading:
                            with self.assertRaises(ValueError):
                                check_run(run,True)
                            changed = self.card()[:-1]+b'\0'
                            with self.assertRaises(ValueError):
                                check_run(run,True,changed)
                for trace,reading in ((saved.replace('sw=9000','sw=9404'),False),
                                      (saved+'\nupdate fid=6f3c record=1 length=176',False),
                                      (cold+'\nupdate fid=6f3c',True),
                                      (cold.replace('ins=b2','ins=c0'),True)):
                    with patch('tools.nsk1_sms_draft_roundtrip.check_phase',return_value=({},trace)):
                        with self.assertRaises(ValueError):
                            check_run(run,reading,self.card())


if __name__ == '__main__':
    unittest.main()
