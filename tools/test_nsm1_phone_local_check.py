import unittest
from unittest.mock import patch

from tools import nsm1_phone_local_check as checker


class PhoneLocalCheckTest(unittest.TestCase):
    def storage(self):
        before = bytearray(0x4000)
        before[0x3d6] = 1
        after = bytearray(before)
        after[0x3d6] = 0
        after[0x3e8] = 6
        after[checker.PHONE:checker.PHONE + len(checker.RECORD)] = checker.RECORD
        return bytes(before), bytes(after)

    def test_writer_changes_only_reviewed_record_and_settings(self):
        before, after = self.storage()
        checker.check_writer_storage(before, after)
        for offset in (0, 0x3d0, 0x1000, checker.PHONE - 1,
                       checker.PHONE + len(checker.RECORD), 0x3fff):
            changed = bytearray(after)
            changed[offset] ^= 1
            with self.subTest(offset=offset), self.assertRaisesRegex(ValueError, 'unrelated'):
                checker.check_writer_storage(before, changed)

    def test_writer_requires_empty_input_and_distinct_name_number(self):
        before, after = self.storage()
        for offset in (checker.PHONE, checker.PHONE + 1, checker.PHONE + 16):
            changed = bytearray(after)
            changed[offset] ^= 1
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                checker.check_writer_storage(before, changed)
        changed = bytearray(before)
        changed[checker.PHONE] = 4
        for initial, saved in ((before[:-1], after), (changed, after), (before, after[:-1])):
            with self.assertRaises(ValueError):
                checker.check_writer_storage(initial, saved)

    def test_cold_reader_must_match_independent_boot_writes(self):
        _, writer = self.storage()
        control = bytearray(writer)
        control[0x3e8] = 0
        control[0x2f85] = 0x3f
        checker.check_cold_storage(writer, control, control)
        changed = bytearray(control)
        changed[0x3100] ^= 1
        with self.assertRaisesRegex(ValueError, 'independent'):
            checker.check_cold_storage(writer, changed, control)
        for offset in (checker.PHONE + 1, 0, 0x3d0):
            changed = bytearray(control)
            changed[offset] ^= 1
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                checker.check_cold_storage(writer, changed, changed)

    def test_writer_requires_authentication_and_complete_physical_contacts(self):
        trace = '\n'.join((
            'nsm1_security_accepted: result=00000001',
            'nsm1_phone_local: mode=write begin=1',
            *[f'nsm1_phone_local_key: key={key:02x} down={down}'
              for key in checker.WRITE_KEYS for down in (1, 0)],
            'nsm1_phone_local: mode=write complete=1'))
        with patch.object(checker, 'check_trace') as verify:
            checker.check_writer_trace(trace)
            verify.assert_called_once_with(trace, True, ())
        for old, new in (('key=03', 'key=02'), ('down=0', 'down=1'), ('down=0', 'down=01'),
                         ('complete=1', 'complete=0'), ('complete=1', 'complete=10')):
            with self.subTest(old=old), patch.object(checker, 'check_trace'), self.assertRaises(ValueError):
                checker.check_writer_trace(trace.replace(old, new))

    def test_reusing_a_capture_is_not_an_independent_control(self):
        from pathlib import Path
        with self.assertRaisesRegex(ValueError, 'independent'):
            checker.check_runs(Path('source'), Path('writer'), Path('reader'), Path('reader'))


if __name__ == '__main__':
    unittest.main()
