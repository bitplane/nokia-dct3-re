import unittest

from tools.run_noki6210_phone_memory_roundtrip import (
    verify_reverse_storage, verify_reverse_trace)
from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH


class PhoneMemoryRoundtripTest(unittest.TestCase):
    def setUp(self):
        card = bytearray(b'\xff' * CURRENT_NVRAM_LENGTH)
        card[0] = ord('A')
        card[18:22] = b'\x03\x81\x21\xf3'
        self.card = bytes(card)
        self.flash = b'\xff' * 0x400000
        self.changed = self.flash[:-1] + b'\x00'
        self.events = [
            'header cla=a0 ins=b2 p1=01 p2=04 p3=20 selected=6f3a',
            *('6210_phone_memory_reverse: action=' + action for action in
              ('sim_to_phone', 'one_by_one', 'contact', 'keep_original', 'copied')),
            '6210_phone_memory_reverse: complete=1 firmware_state_forcing=0']

    def test_readonly_source_and_persistent_destination(self):
        verify_reverse_trace('\n'.join(self.events))
        verify_reverse_storage(self.card, self.card, self.card, self.card,
                               self.flash, self.changed)

    def test_missing_or_reordered_direction_and_source_read_fail(self):
        for index in range(len(self.events)):
            with self.subTest(missing=index), self.assertRaises(ValueError):
                verify_reverse_trace('\n'.join(self.events[:index] + self.events[index+1:]))
        bad = list(self.events)
        bad[1], bad[5] = bad[5], bad[1]
        with self.assertRaises(ValueError):
            verify_reverse_trace('\n'.join(bad))

    def test_source_write_and_destructive_choice_fail(self):
        for extra in ('header cla=a0 ins=dc',
                      '6210_phone_memory_reverse: action=erase_original'):
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                verify_reverse_trace('\n'.join(self.events + [extra]))

    def test_complete_source_preservation_in_every_process(self):
        changed = self.card[:-1] + b'\x00'
        for index in range(1, 4):
            cards = [self.card] * 4
            cards[index] = changed
            with self.subTest(phase=index), self.assertRaises(ValueError):
                verify_reverse_storage(*cards, self.flash, self.changed)

    def test_empty_or_wrong_source_cannot_be_copied(self):
        for source in (b'\xff' * CURRENT_NVRAM_LENGTH, self.card[:-1]):
            with self.subTest(length=len(source)), self.assertRaises(ValueError):
                verify_reverse_storage(*([source] * 4), self.flash, self.changed)

    def test_missing_or_unchanged_handset_flash_fails(self):
        for before, after in ((self.flash, self.flash), (b'', b'x'),
                              (self.flash, self.changed[:-1])):
            with self.subTest(lengths=(len(before), len(after))), self.assertRaises(ValueError):
                verify_reverse_storage(*([self.card] * 4), before, after)


if __name__ == '__main__':
    unittest.main()
