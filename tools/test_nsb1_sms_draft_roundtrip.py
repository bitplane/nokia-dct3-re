import unittest

from tools.nsb1_sms_draft_roundtrip import (DRAFT, RECORD_SIZE, SMS_OFFSET,
                                           check_card, check_contacts, check_editor_state)


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


if __name__ == '__main__':
    unittest.main()
