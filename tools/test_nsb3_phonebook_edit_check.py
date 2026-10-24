import unittest

from tools.nsb3_phonebook_edit_check import check_trace, check_edited_card, edited_record, expected_card, EDIT_KEYS, READ_KEYS
from tools.test_nsb3_phonebook_check import trace


def edit_trace(readback=False):
    text = trace(READ_KEYS if readback else EDIT_KEYS)
    tx = bytes.fromhex('ff00ffa0a40000026f3aa0b2010420')
    if not readback:
        tx += bytes.fromhex('a0dc010420') + edited_record()
        text += '\nSIM status ins=dc sw=9000 chv=3/3 puk=10/10 enabled=0'
    return text + '\n' + '\n'.join(
        f'nsb3_sim_wire: direction=TX data={byte:02x} t=73.0 observer_writes=0' for byte in tx)


class Nsb3PhonebookEditTest(unittest.TestCase):
    def test_name_and_number_are_both_changed(self):
        before, after = expected_card(False), expected_card()
        self.assertEqual([i for i, (a, b) in enumerate(zip(before, after)) if a != b], [0, 22])
        check_edited_card(after)
        with self.assertRaises(ValueError):
            check_edited_card(before)

    def test_edit_and_cold_details_are_distinct(self):
        for readback in (False, True):
            check_trace(edit_trace(readback), readback)
            with self.assertRaises(ValueError):
                check_trace(edit_trace(readback), not readback)

    def test_missing_save_contacts_and_failed_status_rejected(self):
        text = edit_trace()
        for bad in (text.replace('nsb3_phonebook_key:', 'missing_key:'),
                    text.replace('sw=9000', 'sw=9804'),
                    text + '\nnsb3_board_reset_request: data=05'):
            with self.assertRaises(ValueError):
                check_trace(bad)

    def test_other_records_and_credentials_must_stay_unchanged(self):
        for offset in (32, 1600, len(expected_card()) - 1):
            changed = bytearray(expected_card())
            changed[offset] ^= 1
            with self.assertRaises(ValueError):
                check_edited_card(changed)


if __name__ == '__main__':
    unittest.main()
