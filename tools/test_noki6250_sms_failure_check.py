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
