import hashlib
import unittest
from unittest.mock import patch

from tools.nse2_sim_check import BINARY_FILES, verify_sim_reads, verify_security_attempt


def reads():
    rows = []
    for fid, size in BINARY_FILES.items():
        rows.extend((f'sim_device: header cla=a0 ins=b0 p1=00 p2=00 p3={size:02x} selected={fid:04x} t=4.2',
                     'SIM completion ins=b0 sw=9000'))
    for fid, count, size in ((0x6f3a, 50, 32), (0x6f3c, 10, 176)):
        for record in range(1, count + 1):
            rows.extend((f'sim_device: header cla=a0 ins=b2 p1={record:02x} p2=04 p3={size:02x} selected={fid:04x} t=8.8',
                         'SIM completion ins=b2 sw=9000'))
    return '\n'.join(rows)


def contacts():
    rows = []
    for i, (raw, key) in enumerate(((7, 1), (8, 2), (9, 3), (12, 4), (13, 5), (11, 0x19)), 1):
        start = 12 + (i - 1) * 0.5 if i < 6 else 15
        rows.extend((f'nse2_security_contact: index={i} down=1 t={start} physical_only=1',
                     f'nse2_key_decode: raw={raw} key={key:02x} pc=00299044 t={start + 0.1} observer_writes=0',
                     f'nse2_security_contact: index={i} down=0 t={start + 0.2} physical_only=1'))
    return '\n'.join(rows)


class SimCheckTest(unittest.TestCase):
    def check_reads(self, text):
        with patch('tools.nse2_sim_check.SECURITY_FRAME', hashlib.sha256(b'frame').hexdigest()):
            return verify_sim_reads(text, b'frame')

    def test_complete_reads_do_not_claim_authentication(self):
        result = self.check_reads(reads())
        self.assertEqual(result['adn_records_read'], 50)
        self.assertEqual(result['sms_records_read'], 10)
        self.assertFalse(result['phone_code_authenticated'])
        self.assertFalse(result['sim_pin_verified'])

    def test_bad_status_header_record_length_and_horizon_fail(self):
        for old, new in (('sw=9000', 'sw=9404'), ('cla=a0', 'cla=00'),
                         ('p1=32', 'p1=31'), ('p3=b0', 'p3=af'), ('t=8.8', 't=11.0')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                self.check_reads(reads().replace(old, new))

    def test_pin_command_and_wrong_frame_fail(self):
        with self.assertRaisesRegex(ValueError, 'VERIFY CHV'):
            self.check_reads(reads() + '\nsim_device: header cla=a0 ins=20 p1=00 p2=01 p3=08 selected=6f3a t=9.0')
        with self.assertRaisesRegex(ValueError, 'frame differs'):
            verify_sim_reads(reads(), b'wrong')

    def test_one_rejected_physical_attempt_is_narrowly_scoped(self):
        with patch('tools.nse2_sim_check.CODE_ERROR_FRAME', hashlib.sha256(b'frame').hexdigest()):
            result = verify_security_attempt(contacts(), b'frame')
        self.assertEqual(result['physical_phone_code_attempts'], 1)
        self.assertFalse(result['phone_code_authenticated'])
        self.assertTrue(result['exact_editor_buffer_unproved'])

    def test_missing_extra_wrong_and_outside_window_contacts_fail(self):
        for text in (contacts().replace('key=01', 'key=02'),
                     contacts().replace('t=12.1', 't=11.1'),
                     contacts() + '\nnse2_security_contact: index=1 down=1 t=18 physical_only=1',
                     contacts().replace('t=12.2', 't=12.5')):
            with self.subTest(text=text[:70]), self.assertRaises(ValueError):
                verify_security_attempt(text, b'frame')


if __name__ == '__main__':
    unittest.main()
