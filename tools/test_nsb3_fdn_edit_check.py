import unittest

from tools.nsb3_fdn_edit_check import EDIT_KEYS, edited_record, expected_card, check_card, original_card
from tools.nsb3_fdn_record_check import READ_KEYS, check_record_trace
from tools.test_nsb3_fdn_record_check import record_trace


def edit_trace():
    lines = [line for line in record_trace().splitlines() if not line.startswith('nsb3_security_key:')]
    lines += [f'nsb3_security_key: key={key} down={state} t=66.0 physical_only=1'
              for key in EDIT_KEYS for state in (1, 0)]
    return '\n'.join(lines).replace('data=f5 ', 'data=f6 ')


class Nsb3FdnEditTest(unittest.TestCase):
    def test_exact_protected_in_place_edit(self):
        check_record_trace(edit_trace(), EDIT_KEYS, edited_record())
        check_record_trace(record_trace(True), READ_KEYS)
        with self.assertRaises(ValueError):
            check_record_trace(edit_trace(), READ_KEYS)

    def test_only_final_fdn_digit_changes(self):
        before, after = original_card(), expected_card()
        self.assertEqual([(i, a, b) for i, (a, b) in enumerate(zip(before, after)) if a != b], [(3600, 0xf5, 0xf6)])
        check_card(after)
        for offset in (0, 3517, 3575, 3578, 3610):
            changed = bytearray(after)
            changed[offset] ^= 1
            with self.assertRaises(ValueError):
                check_card(changed)

    def test_wrong_payload_status_or_missing_physical_actions_fails(self):
        text = edit_trace()
        for bad in (text.replace('data=f6 ', 'data=f5 '),
                    text.replace('ins=dc sw=9000', 'ins=dc sw=9804'),
                    text.replace('nsb3_security_key:', 'missing_key:')):
            with self.assertRaises(ValueError):
                check_record_trace(bad, EDIT_KEYS, edited_record())


if __name__ == '__main__':
    unittest.main()
