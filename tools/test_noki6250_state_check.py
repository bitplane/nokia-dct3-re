import unittest
from unittest.mock import patch

from tools.noki6250_state_check import verify
from tools.test_noki8210_state_check import GOOD as IDLE_TRACE
from tools.test_noki6250_sms_check import LOG as SMS_LOG
from tools.radio_sms_acceptance_common import FIRST_SMS_DELIVER_BODY, SMS_NVRAM_OFFSET


GOOD = IDLE_TRACE.replace('8210_', '6250_').replace(
    '6250_keypad_decoded: key=19', '6250_raw_matrix_key: value=06')
SMS_STORAGE = bytes(SMS_NVRAM_OFFSET) + bytes([1]) + FIRST_SMS_DELIVER_BODY + bytes(176)


class Nokia6250StateTest(unittest.TestCase):
    def sms_trace(self):
        before, after = SMS_LOG.rsplit('sim_device: update', 1)
        return (before + GOOD.replace('key=Menu', 'key=Read') +
                'sim_device: update' + after)

    def test_sms_restoration(self):
        verify(self.sms_trace(), sms=True, storage=SMS_STORAGE)

    def test_sms_requires_storage(self):
        with self.assertRaisesRegex(ValueError, 'persistent SIM storage'):
            verify(self.sms_trace(), sms=True)

    def test_sms_rejects_duplicate_record_write(self):
        with self.assertRaisesRegex(ValueError, 'delivery and read-status writes'):
            verify(self.sms_trace() + 'sim_device: update fid=6f3c record=1 length=176\n',
                   sms=True, storage=SMS_STORAGE)

    def test_sms_requires_post_load_read(self):
        with self.assertRaisesRegex(ValueError, 'physical Read'):
            verify(self.sms_trace().replace('key=Read', 'key=Menu'),
                   sms=True, storage=SMS_STORAGE)

    def test_call_save_requires_established_connection(self):
        trace = GOOD + '6250_state_physical: key=End\n'
        with patch('tools.noki6250_state_check.verify_outgoing'):
            with self.assertRaisesRegex(ValueError, 'established active call'):
                verify(trace, call=True)
            verify('GSM service uplink sapi=0 pd=03 message=0f length=2 data=030f\n' + trace,
                   call=True)

    def test_call_requires_post_load_end(self):
        trace = 'GSM service uplink sapi=0 pd=03 message=0f length=2 data=030f\n' + GOOD
        with patch('tools.noki6250_state_check.verify_outgoing'):
            with self.assertRaisesRegex(ValueError, 'physical call release'):
                verify(trace, call=True)

    def test_exact_replay(self):
        verify(GOOD)

    def test_fresh_sip_follows_idle_restoration(self):
        trace = GOOD + ('6250_state: restored_idle_for_fresh_sip\n'
                        '6250_sip_cancel: ready\n'
                        'GSM service downlink kind=9 sapi=0 pd=03 message=05\n')
        verify(trace, fresh_sip=True)
        for token in ('6250_state: restored_idle_for_fresh_sip', '6250_sip_cancel: ready'):
            with self.assertRaisesRegex(ValueError, 'completed idle restoration'):
                verify(trace.replace(token, 'missing'), fresh_sip=True)

    def test_fresh_sip_not_active_call_or_sms_restore(self):
        for options in ({'call': True}, {'sms': True}):
            with self.assertRaisesRegex(ValueError, 'requires idle'):
                verify(GOOD, fresh_sip=True, **options)

    def test_architectural_mismatch(self):
        with self.assertRaisesRegex(ValueError, 'architectural state'):
            verify(GOOD.replace('restored pc=0000001c', 'restored pc=00000020'))

    def test_empty_replay_is_not_evidence(self):
        with self.assertRaisesRegex(ValueError, 'no radio records'):
            verify('\n'.join(line for line in GOOD.splitlines() if 'TX packet' not in line))

    def test_wrong_physical_scan(self):
        with self.assertRaisesRegex(ValueError, 'physical Menu'):
            verify(GOOD.replace('value=06', 'value=07'))

    def test_incomplete_fixture(self):
        with self.assertRaisesRegex(ValueError, 'did not complete'):
            verify(GOOD + '6250_state: FAIL incomplete')
