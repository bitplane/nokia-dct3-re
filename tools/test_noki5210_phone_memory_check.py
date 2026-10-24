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
                checker.verify(text, card, mode='read' if read else 'save')
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
            checker.verify(self.capture(), checker.expected_card(), mode='read')

    def test_preserved_modes_have_distinct_contracts(self):
        for mode in ('edit', 'edit_read', 'erase_cancel', 'erase', 'empty_read', 'rename', 'rename_read',
                     'add', 'multi_read', 'erase_first'):
            text = '\n'.join((
                'SIM status ins=20 sw=9000', 'LAPDm Location Updating Accept acknowledged',
                f'5210_phone_memory: mode={mode} begin=1',
                *[f'5210_phone_memory_key: name={key} released=1' for key in checker.KEYS[mode]],
                f'5210_phone_memory: mode={mode} complete=1'))
            with self.subTest(mode=mode), patch.object(checker, 'verify_coherent_late') as verify:
                checker.verify(text, checker.expected_card(), mode=mode)
                verify.assert_called_once_with(text, checker.expected_card(), preserved=True)
                with self.assertRaises(ValueError):
                    checker.verify(text, checker.expected_card(), mode='read')
        with self.assertRaises(ValueError):
            checker.verify('', checker.expected_card(), mode='unreviewed')

    def test_erase_decision_and_empty_storage_inspection_are_required(self):
        cases = (
            ('erase_cancel', checker.ERASE_PREFIX + ['Left Softkey / Menu'] + checker.READ_KEYS[-3:]),
            ('erase', checker.ERASE_PREFIX + ['Right Softkey / C'] + checker.READ_KEYS[-3:]),
            ('erase_first', checker.ERASE_PREFIX + ['Right Softkey / C'] + checker.READ_KEYS[-3:]),
            ('empty_read', checker.READ_KEYS[:3] + checker.READ_KEYS[-3:]),
        )
        for mode, keys in cases:
            text = '\n'.join((
                'SIM status ins=20 sw=9000', 'LAPDm Location Updating Accept acknowledged',
                f'5210_phone_memory: mode={mode} begin=1',
                *[f'5210_phone_memory_key: name={key} released=1' for key in keys],
                f'5210_phone_memory: mode={mode} complete=1'))
            with self.subTest(mode=mode), patch.object(checker, 'verify_coherent_late'), \
                    self.assertRaisesRegex(ValueError, 'input/release'):
                checker.verify(text, checker.expected_card(), mode=mode)

    def test_authentication_precedes_phone_storage(self):
        text = self.capture().replace('SIM status ins=20 sw=9000', '') + '\nSIM status ins=20 sw=9000'
        with patch.object(checker, 'verify_coherent_late'), self.assertRaises(ValueError):
            checker.verify(text, checker.expected_card())

    def test_rename_requires_replace_and_a_distinct_name_frame(self):
        self.assertEqual(checker.MODE_FRAMES['rename_read']['phonebook'], checker.READ_FRAMES['phonebook'])
        self.assertNotEqual(checker.MODE_FRAMES['rename_read']['phonebook_list'], checker.READ_FRAMES['phonebook_list'])
        self.assertEqual(checker.KEYS['rename'][-6:-3], ['Left Softkey / Menu'] * 3)
        keys = checker.KEYS['rename'].copy()
        keys.insert(len(keys) - 4, 'Scroll Down')  # Select Add instead of Replace.
        text = '\n'.join((
            'SIM status ins=20 sw=9000', 'LAPDm Location Updating Accept acknowledged',
            '5210_phone_memory: mode=rename begin=1',
            *[f'5210_phone_memory_key: name={key} released=1' for key in keys],
            '5210_phone_memory: mode=rename complete=1'))
        with patch.object(checker, 'verify_coherent_late'), self.assertRaisesRegex(ValueError, 'input/release'):
            checker.verify(text, checker.expected_card(), mode='rename')

    def test_second_contact_requires_add_and_both_selections(self):
        self.assertNotEqual(checker.KEYS['add'], checker.KEYS['rename'])
        frames = checker.MODE_FRAMES['multi_read']
        self.assertNotEqual(frames['phonebook_list'], frames['second_list'])
        self.assertEqual(frames['phonebook'], frames['second_number'])
        for mode, wrong in (('add', checker.KEYS['rename']),
                            ('multi_read', checker.READ_KEYS)):
            text = '\n'.join((
                'SIM status ins=20 sw=9000', 'LAPDm Location Updating Accept acknowledged',
                f'5210_phone_memory: mode={mode} begin=1',
                *[f'5210_phone_memory_key: name={key} released=1' for key in wrong],
                f'5210_phone_memory: mode={mode} complete=1'))
            with self.subTest(mode=mode), patch.object(checker, 'verify_coherent_late'), \
                    self.assertRaisesRegex(ValueError, 'input/release'):
                checker.verify(text, checker.expected_card(), mode=mode)

    def test_selective_erase_uses_different_selected_name(self):
        frames = checker.MODE_FRAMES['erase_first']
        self.assertEqual(frames['phonebook_list'], checker.MODE_FRAMES['multi_read']['phonebook_list'])
        self.assertNotEqual(frames['phonebook_list'], checker.READ_FRAMES['phonebook_list'])
        self.assertNotEqual(frames['erase_prompt'], checker.ERASE_PROMPT)
        self.assertNotEqual(frames['erase_result'], checker.MODE_FRAMES['erase']['erase_result'])
