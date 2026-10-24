import unittest

from tools.run_noki6210_phone_memory_copy import verify_copy_storage, verify_copy_trace
from tools.run_noki6210_phone_memory_copy import (
    verify_bank_selection_digests, SELECTED_SHA256, SIM_SELECTED_SHA256)
from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH, validate_phonebook


class PhoneMemoryCopyTest(unittest.TestCase):
    def test_reading_same_contact_twice_cannot_replace_bank_selection(self):
        verify_bank_selection_digests({'phone': SELECTED_SHA256, 'sim': SIM_SELECTED_SHA256})
        for bad in ({'phone': SIM_SELECTED_SHA256, 'sim': SELECTED_SHA256},
                    {'phone': SELECTED_SHA256, 'sim': SELECTED_SHA256},
                    {'phone': SELECTED_SHA256}):
            with self.subTest(actual=bad), self.assertRaises(ValueError):
                verify_bank_selection_digests(bad)

    def setUp(self):
        self.before = b'\xff' * CURRENT_NVRAM_LENGTH
        record = bytearray(b'\xff' * 32)
        record[0] = ord('A')
        record[18:22] = b'\x03\x81\x21\xf3'
        self.copied = bytes(record) + self.before[32:]
        self.events = (
            '6210_phone_memory_copy: action=keep_original',
            '6210_phone_memory_copy: action=copied',
            'header cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3a',
            'body ins=dc length=32 selected=6f3a',
            'update fid=6f3a record=1 length=32',
            'SIM status ins=dc sw=9000',
            '6210_phone_memory_copy: complete=1 firmware_state_forcing=0')

    def test_copy_keeps_entire_unrelated_card_and_cold_destinations(self):
        trace = '\n'.join(self.events)
        verify_copy_trace(trace)
        validate_phonebook(trace, self.copied, b'A')
        verify_copy_storage(self.before, self.copied, self.copied, self.copied)

    def test_missing_or_reordered_physical_commit_fails(self):
        for i in (0, 1, 2, 3, 5, 6):
            with self.subTest(missing=i), self.assertRaises(ValueError):
                verify_copy_trace('\n'.join(self.events[:i] + self.events[i+1:]))
        bad = list(self.events)
        bad[0], bad[5] = bad[5], bad[0]
        with self.assertRaises(ValueError):
            verify_copy_trace('\n'.join(bad))

    def test_destructive_choice_must_be_explicit_and_ordered(self):
        trace = '\n'.join(self.events)
        with self.assertRaises(ValueError):
            verify_copy_trace(trace, move=True)
        events = list(self.events)
        events.insert(1, '6210_phone_memory_copy: action=erase_original')
        verify_copy_trace('\n'.join(events), move=True)
        with self.assertRaises(ValueError):
            verify_copy_trace('\n'.join(events))

    def test_unrelated_record_or_security_mutation_fails(self):
        for offset in (32, 1599, 1600, CURRENT_NVRAM_LENGTH - 1):
            bad = bytearray(self.copied)
            bad[offset] ^= 1
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                verify_copy_storage(self.before, bytes(bad), bytes(bad), bytes(bad))

    def test_cold_loss_or_mutation_fails(self):
        for phone, sim in ((self.before, self.copied), (self.copied, self.before),
                           (self.copied[:-1], self.copied)):
            with self.subTest(lengths=(len(phone), len(sim))), self.assertRaises(ValueError):
                verify_copy_storage(self.before, self.copied, phone, sim)

    def test_nonempty_or_truncated_predecessor_fails(self):
        for before in (b'', self.before[:-1], self.copied):
            with self.subTest(length=len(before)), self.assertRaises(ValueError):
                verify_copy_storage(before, self.copied, self.copied, self.copied)

    def test_skipped_copy_or_wrong_number_fails(self):
        trace = '\n'.join(self.events)
        with self.assertRaises(ValueError):
            verify_copy_storage(self.before, self.before, self.before, self.before)
        with self.assertRaises(ValueError):
            validate_phonebook(trace, self.before, b'A')
        bad = bytearray(self.copied)
        bad[21] = 0xf4
        with self.assertRaises(ValueError):
            validate_phonebook(trace, bytes(bad), b'A')


if __name__ == '__main__':
    unittest.main()
