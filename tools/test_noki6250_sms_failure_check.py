import unittest
from tools.noki6250_sms_failure_check import verify

GOOD = '''6250_sms_input: step=25 pressed=1
GSM service uplink sapi=3 pd=09 message=01 length=28 data=390119000100069121436587090e11010781551532f40000ff02c834
gsm_sms_submit: cp=39
gsm_call_adapter: sms decision id=2 outcome=1 result=rejected
gsm_call_adapter: sms decision id=1 outcome=1 result=accepted
gsm_call_adapter: sms decision id=1 outcome=1 result=rejected
GSM service downlink kind=19 sapi=3 pd=09 message=01 length=7
GSM service uplink sapi=3 pd=09 message=04 length=2 data=3904
LAPDm service Channel Release acknowledged
6250_sms_recovery_physical: key=End
6250_raw_matrix_key: value=0f
6250_sms_recovery_physical: key=End
6250_raw_matrix_key: value=0f
6250_sms_recovery_physical: key=Left Softkey / Menu
6250_raw_matrix_key: value=06
'''


class FailureCheckTest(unittest.TestCase):
    def silence_trace(self):
        return GOOD.split('gsm_call_adapter: sms decision', 1)[0] + '''GSM service downlink kind=17 sapi=3 pd=09 message=04
gsm_call_adapter: sms decision id=1 outcome=3 result=accepted
TX packet type=1b data=0080015301
RX enqueue type=80 data=80000000000000000000017301
TX packet type=02 radio_phase=release_channel_change
gsm_call_adapter: sms state id=1 epoch=1 phase=ended
PCH no-identity fill
''' + '6250_sms_recovery_physical:' + GOOD.split('6250_sms_recovery_physical:', 1)[1]

    def test_silence_recovery(self):
        verify(self.silence_trace(), rp_silence=True)

    def test_silence_forbids_rp_result(self):
        with self.assertRaisesRegex(ValueError, 'RP error'):
            verify(self.silence_trace() + 'GSM service downlink kind=19 sapi=3\n', rp_silence=True)

    def test_silence_requires_host_closure(self):
        with self.assertRaisesRegex(ValueError, 'correlated host end'):
            verify(self.silence_trace().replace('phase=ended', 'phase=queued'), rp_silence=True)

    def test_rejection_and_recovery(self):
        verify(GOOD)

    def test_wrong_message(self):
        with self.assertRaisesRegex(ValueError, 'exact Hi'):
            verify(GOOD.replace('ff02c834', 'ff02c824'))

    def test_success_not_rejection(self):
        with self.assertRaisesRegex(ValueError, 'success RP'):
            verify(GOOD + 'GSM service downlink kind=18 sapi=3\n')

    def test_requires_second_end(self):
        with self.assertRaisesRegex(ValueError, 'second End'):
            verify(GOOD.replace('6250_sms_recovery_physical: key=End\n', '', 1))

    def test_requires_menu_scan(self):
        with self.assertRaisesRegex(ValueError, 'Menu scan'):
            verify(GOOD.replace('value=06', 'value=07'))
