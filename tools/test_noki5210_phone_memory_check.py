import unittest
from unittest.mock import patch

from tools import noki5210_phone_memory_check as checker


class PhoneMemoryCheckTest(unittest.TestCase):
    def capture(self, read=False):
        mode = 'read' if read else 'save'
        return '\n'.join((
            'SIM status ins=20 sw=9000', 'LAPDm Location Updating Accept acknowledged',
            f'5210_phone_memory: mode={mode} begin=1',
            *[f'5210_phone_memory_key: name={key} released=1'
              for key in (checker.READ_KEYS if read else checker.SAVE_KEYS)],
            f'5210_phone_memory: mode={mode} complete=1'))

    def test_product_registration_scope_is_preserved(self):
        for read in (False, True):
            text, card = self.capture(read), checker.expected_card()
            with self.subTest(read=read), patch.object(checker, 'verify_coherent_late') as verify:
                checker.verify(text, card, read=read)
                verify.assert_called_once_with(text, card, preserved=read)

    def test_keys_and_completion_are_not_optional(self):
        text, card = self.capture(), checker.expected_card()
        for old, new in [('name=Keypad 3', 'name=Keypad 9'), ('released=1', 'released=0'),
                         ('complete=1', 'complete=0'), ('begin=1', 'begin=0')]:
            with self.subTest(old=old), patch.object(checker, 'verify_coherent_late'), self.assertRaises(ValueError):
                checker.verify(text.replace(old, new), card)
        with patch.object(checker, 'verify_coherent_late'), self.assertRaises(ValueError):
            checker.verify(text + '\n5210_phone_memory: mode=save complete=1', card)

    def test_whole_sim_and_attempted_record_writes_reject(self):
        text, card = self.capture(), checker.expected_card()
        for modified in (card[:-1], card + b'\xff', bytes([card[0] ^ 1]) + card[1:]):
            with patch.object(checker, 'verify_coherent_late'), self.assertRaises(ValueError):
                checker.verify(text, modified)
        for extra in ('sim_device: header cla=a0 ins=dc p1=01', 'sim_device: update fid=6f3a record=1'):
            with patch.object(checker, 'verify_coherent_late'), self.assertRaises(ValueError):
                checker.verify(text + '\n' + extra, card)

    def test_cold_read_does_not_accept_save_sequence(self):
        with patch.object(checker, 'verify_coherent_late'), self.assertRaises(ValueError):
            checker.verify(self.capture(), checker.expected_card(), read=True)

    def test_authentication_precedes_phone_storage(self):
        text = self.capture().replace('SIM status ins=20 sw=9000', '') + '\nSIM status ins=20 sw=9000'
        with patch.object(checker, 'verify_coherent_late'), self.assertRaises(ValueError):
            checker.verify(text, checker.expected_card())
