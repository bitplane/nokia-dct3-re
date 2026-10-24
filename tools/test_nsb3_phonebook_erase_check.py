import unittest

from tools.nsb3_phonebook_erase_check import check_trace, check_empty_card, ERASE_KEYS, EMPTY_KEYS
from tools.test_nsb3_phonebook_check import trace
from tools.make_sim_card_profile import make_current_profile


def erase_trace(empty=False):
    text = trace(EMPTY_KEYS if empty else ERASE_KEYS)
    tx = bytes.fromhex('ff00ffa0a40000026f3aa0b2010420')
    if not empty:
        tx += bytes.fromhex('a0dc010420') + b'\xff' * 32
        text += '\nSIM status ins=dc sw=9000 chv=3/3 puk=10/10 enabled=0'
    return text + '\n' + '\n'.join(
        f'nsb3_sim_wire: direction=TX data={byte:02x} t=58.0 observer_writes=0' for byte in tx)


class Nsb3PhonebookEraseTest(unittest.TestCase):
    def test_erase_and_read_only_cold_search_are_not_interchangeable(self):
        for empty in (False, True):
            check_trace(erase_trace(empty), empty)
            with self.assertRaises(ValueError):
                check_trace(erase_trace(empty), not empty)

    def test_wrong_status_missing_contacts_and_reset_fail(self):
        text = erase_trace()
        for bad in (text.replace('sw=9000', 'sw=9804'),
                    text.replace('nsb3_phonebook_key:', 'missing_key:'),
                    text + '\nnsb3_board_reset_request: data=05'):
            with self.assertRaises(ValueError):
                check_trace(bad)

    def test_empty_card_must_retain_every_unrelated_byte(self):
        card = make_current_profile(False)
        check_empty_card(card)
        for offset in (0, 32, len(card) - 1):
            changed = bytearray(card)
            changed[offset] ^= 1
            with self.assertRaises(ValueError):
                check_empty_card(changed)


if __name__ == '__main__':
    unittest.main()
