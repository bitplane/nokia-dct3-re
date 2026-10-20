import unittest
from unittest.mock import patch
from tools.nsk1_fdn_record_check import expected_card, edited_card, erased_card, check_trace, RECORD
from tools.nsk1_puk_roundtrip import exhaustion_profile
from tools.sim_fdn_profile import extend
from tools.sim_security_trace_check import ATTEMPTS_OFFSET, padded_pin


class Nsk1FdnRecordTests(unittest.TestCase):
    def test_save_and_rejection_have_disjoint_exact_changes(self):
        before = extend(exhaustion_profile(), True)
        saved = expected_card(before)
        self.assertEqual(saved[3578:3610], RECORD)
        self.assertEqual(saved[:3578], before[:3578])
        self.assertEqual(saved[3610:], before[3610:])
        rejected = expected_card(before, True)
        self.assertEqual([i for i, pair in enumerate(zip(before, rejected)) if pair[0] != pair[1]], [ATTEMPTS_OFFSET+1])
        self.assertEqual(rejected[ATTEMPTS_OFFSET+1], 2)
        for invalid in (before[:-1], saved, rejected):
            with self.assertRaises(ValueError):
                expected_card(invalid)

    def transaction(self):
        return (bytes.fromhex('a0a40000026f3ba020000108') + padded_pin('1234')
                + bytes.fromhex('a020000208') + padded_pin('5678')
                + bytes.fromhex('a0dc010420') + RECORD)

    def trace(self):
        return ('SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1\n'*2
                + 'SIM status ins=dc sw=9000 chv=3/3 puk=10/10 enabled=1\n'
                + 'sim_device: update fid=6f3b record=1 length=32')

    def test_save_requires_correct_file_and_command_payload(self):
        with patch('tools.nsk1_fdn_record_check.check_input'), \
             patch('tools.nsk1_fdn_record_check.transmitted', return_value=self.transaction()):
            check_trace(self.trace(), 'write')
            with self.assertRaises(ValueError):
                check_trace(self.trace().replace('fid=6f3b', 'fid=6f3a'), 'write')
        with patch('tools.nsk1_fdn_record_check.check_input'), \
             patch('tools.nsk1_fdn_record_check.transmitted', return_value=self.transaction()[:-1]+b'\x00'):
            with self.assertRaises(ValueError):
                check_trace(self.trace(), 'write')

    def test_no_truncated_observation_is_accepted(self):
        with patch('tools.nsk1_fdn_record_check.check_input'), \
             patch('tools.nsk1_fdn_record_check.transmitted', return_value=self.transaction()+bytes(4096)):
            with self.assertRaises(ValueError):
                check_trace(self.trace(), 'write')

    def test_edit_replaces_only_last_bcd_digit_in_existing_record(self):
        saved = expected_card(extend(exhaustion_profile(), True))
        edited = edited_card(saved)
        self.assertEqual([i for i, pair in enumerate(zip(saved, edited)) if pair[0] != pair[1]], [3600])
        self.assertEqual(edited[3600], 0xf6)
        for invalid in (edited, saved[:-1], extend(exhaustion_profile(), True)):
            with self.assertRaises(ValueError):
                edited_card(invalid)
        record = bytearray(RECORD)
        record[22] = 0xf6
        with patch('tools.nsk1_fdn_record_check.check_input'), \
             patch('tools.nsk1_fdn_record_check.transmitted', return_value=self.transaction()[:-32]+record):
            check_trace(self.trace(), 'edit')
        with patch('tools.nsk1_fdn_record_check.check_input'), \
             patch('tools.nsk1_fdn_record_check.transmitted', return_value=self.transaction()):
            with self.assertRaises(ValueError):
                check_trace(self.trace(), 'edit')

    def test_erase_requires_edited_record_and_clears_only_its_bytes(self):
        initial = extend(exhaustion_profile(), True)
        saved = expected_card(initial)
        edited = edited_card(saved)
        self.assertEqual(erased_card(edited), initial)
        for invalid in (initial, saved, edited[:-1]):
            with self.assertRaises(ValueError):
                erased_card(invalid)
        with patch('tools.nsk1_fdn_record_check.check_input'), \
             patch('tools.nsk1_fdn_record_check.transmitted', return_value=self.transaction()[:-32]+b'\xff'*32):
            check_trace(self.trace(), 'erase')

    def test_cancellation_rejects_even_an_unacknowledged_update(self):
        trace = 'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1\n'*2
        tx = self.transaction()[:-37]
        with patch('tools.nsk1_fdn_record_check.check_input'), \
             patch('tools.nsk1_fdn_record_check.transmitted', return_value=tx):
            check_trace(trace, 'cancel')
        with patch('tools.nsk1_fdn_record_check.check_input'), \
             patch('tools.nsk1_fdn_record_check.transmitted', return_value=self.transaction()):
            with self.assertRaises(ValueError):
                check_trace(trace, 'cancel')


if __name__ == '__main__':
    unittest.main()
