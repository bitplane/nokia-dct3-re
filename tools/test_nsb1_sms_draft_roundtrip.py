import unittest
from pathlib import Path
from unittest.mock import patch

from tools.nsb1_sms_draft_roundtrip import (DRAFT, RECORD_SIZE, SMS_OFFSET,
                                           check_card, check_contacts, check_editor_state,
                                           check_deleted_card, check_erased, check_empty)


class DraftTest(unittest.TestCase):
    def test_state_requires_matching_snapshot_and_single_completed_roundtrip(self):
        saved = 'nsb1_sms_state: event=saved t=41.500000000 pc=002a4672 ram=f20de5ff dsp=PC:47a1,ILLEGAL:0\n'
        restored = saved.replace('event=saved', 'event=restored result=pass')
        check_editor_state(saved + restored)
        for trace in (saved, restored, saved+restored+restored,
                      saved+restored.replace('47a1', '47a2'),
                      saved+restored+'nsb1_sms_state: FAIL incomplete\n'):
            with self.assertRaisesRegex(ValueError, 'save-state'):
                check_editor_state(trace)

    def card(self):
        data = bytearray(3570)
        data[SMS_OFFSET:SMS_OFFSET+RECORD_SIZE] = DRAFT
        data[SMS_OFFSET+RECORD_SIZE:SMS_OFFSET+2*RECORD_SIZE] = b'\x00' + b'\xff' * 175
        return data

    def test_exact_unsent_body_and_free_second_slot(self):
        data = self.card()
        self.assertEqual(176, len(DRAFT))
        check_card(data)
        for index in (SMS_OFFSET, SMS_OFFSET+11, SMS_OFFSET+176):
            changed = bytearray(data)
            changed[index] ^= 1
            with self.assertRaises(ValueError):
                check_card(changed)
        with self.assertRaisesRegex(ValueError, 'complete'):
            check_card(data[:-1])

    def test_physical_reload_order_and_release_required(self):
        lines = [f'nsb1_sms_reload_contact: key={key} down={down} t=26.000 physical_only=1'
                 for key in ('menu','two','one','two','menu') for down in (1,0)]
        check_contacts('\n'.join(lines), reload=True)
        with self.assertRaisesRegex(ValueError, 'physical'):
            check_contacts('\n'.join(lines[:-1]), reload=True)
        with self.assertRaisesRegex(ValueError, 'physical'):
            check_contacts('\n'.join(reversed(lines)), reload=True)

    def reload_trace(self):
        return '\n'.join(f'nsb1_sms_reload_contact: key={key} down={down} t=26.000 physical_only=1'
                         for key in ('menu','two','one','two','menu') for down in (1,0))

    def deleted(self):
        card = self.card()
        card[SMS_OFFSET] = 0
        return bytes(card)

    def test_deletion_is_status_only_not_payload_scrubbing(self):
        check_deleted_card(self.deleted())
        for card in (self.card(),self.deleted()[:-1]):
            with self.assertRaises(ValueError):
                check_deleted_card(card)
        scrubbed = bytearray(self.deleted())
        scrubbed[SMS_OFFSET+1:SMS_OFFSET+RECORD_SIZE] = b'\xff'*(RECORD_SIZE-1)
        with self.assertRaises(ValueError):
            check_deleted_card(scrubbed)

    def test_physical_erase_requires_confirmation_commit_and_success(self):
        trace = self.reload_trace()+'\n'+'\n'.join(
            f'nsb1_sms_erase_contact: index={index} down={down} t=40.0 physical_only=1'
            for index in (1,2,3) for down in (1,0))
        trace += '\nins=dc p1=01 p2=04 p3=b0 selected=6f3c\nupdate fid=6f3c record=1 length=176\nSIM status ins=dc sw=9000'
        with patch('tools.nsb1_sms_draft_roundtrip.check_frame') as frame:
            check_erased(trace,self.deleted(),Path('.'))
            self.assertEqual(frame.call_count,3)
            for bad in (trace.replace('index=3','index=2'),trace.replace('sw=9000','sw=9804'),
                        trace+'\nupdate fid=6f3c record=2 length=176'):
                with self.subTest(trace=bad),self.assertRaises(ValueError):
                    check_erased(bad,self.deleted(),Path('.'))

    def test_empty_outbox_requires_read_without_any_update(self):
        trace = self.reload_trace()+'\nins=b2 p1=01 p2=04 p3=b0 selected=6f3c'
        with patch('tools.nsb1_sms_draft_roundtrip.check_frame') as frame:
            check_empty(trace,self.deleted(),Path('.'))
            frame.assert_called_once()
            for bad in (trace.replace('ins=b2','ins=f2'),trace+'\nins=dc'):
                with self.subTest(trace=bad),self.assertRaises(ValueError):
                    check_empty(bad,self.deleted(),Path('.'))


if __name__ == '__main__':
    unittest.main()
