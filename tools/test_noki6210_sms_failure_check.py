import unittest
from unittest.mock import patch
from tools.noki6210_sms_failure_check import verify
from tools.test_noki8850_outgoing_sms_check import SILENCE

GOOD = '''gsm_call_adapter: sms decision id=2 outcome=1 result=rejected
gsm_call_adapter: sms decision id=1 outcome=1 result=accepted
gsm_call_adapter: sms decision id=1 outcome=1 result=rejected
GSM service downlink kind=19 sapi=3 pd=09 message=01 length=7
LAPDm service Channel Release acknowledged
6210_sms_recovery_physical: key=End
6210_keypad_decoded: key=0f
6210_sms_recovery_physical: key=End
6210_keypad_decoded: key=0f
6210_sms_recovery_physical: key=Left Softkey / Menu
6210_keypad_decoded: key=19
'''


class FailureTest(unittest.TestCase):
    def silence(self):
        return (SILENCE.replace('8850_', '6210_').replace('00a70141', '00ff0141') + '\n' +
                GOOD[GOOD.index('6210_sms_recovery_physical:'):])

    def test_own_silence_contract(self):
        verify(self.silence(), rp_silence=True)

    def test_sibling_validity_not_accepted(self):
        with self.assertRaisesRegex(ValueError, 'exact submission'):
            verify(self.silence().replace('00ff0141', '00a70141'), rp_silence=True)

    def test_timeout_requires_correlated_end(self):
        with self.assertRaisesRegex(ValueError, 'correlated host end'):
            verify(self.silence().replace('phase=ended', 'phase=queued'), rp_silence=True)

    def test_own_submission_and_recovery(self):
        with patch('tools.noki6210_sms_failure_check.verify_submission') as submit:
            verify(GOOD)
            submit.assert_called_once_with(GOOD, rejected=True)

    def test_unrelated_host_request_cannot_pass(self):
        with patch('tools.noki6210_sms_failure_check.verify_submission'):
            with self.assertRaisesRegex(ValueError, 'host error accepted'):
                verify(GOOD.replace('id=1 outcome=1 result=accepted', 'id=2 outcome=1 result=accepted'))

    def test_press_without_decode_cannot_pass(self):
        with patch('tools.noki6210_sms_failure_check.verify_submission'):
            with self.assertRaisesRegex(ValueError, 'Menu decode'):
                verify(GOOD.replace('key=19', 'key=18'))
