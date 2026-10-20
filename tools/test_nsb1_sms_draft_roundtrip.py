import unittest

from tools.nsb1_sms_draft_roundtrip import (DRAFT, RECORD_SIZE, SMS_OFFSET,
                                           check_card, check_contacts)


class DraftTest(unittest.TestCase):
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
