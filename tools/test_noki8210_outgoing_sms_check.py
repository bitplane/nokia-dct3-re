import unittest

try:
    from tools.noki8210_outgoing_sms_check import verify
except ModuleNotFoundError:
    from noki8210_outgoing_sms_check import verify


class OutgoingSmsTest(unittest.TestCase):
    def test_repeat_submission_reference(self):
        trace = '\n'.join((
            '8210_sms_send_physical: action=text_A',
            '8210_sms_send_physical: action=confirm_send',
            '8210_keypad_decoded: key=19',
            'GSM service uplink sapi=3 pd=09 message=01 length=27 data=390118000100069121436587090d11020781551532f40000a70141',
            'gsm_sms_submit: cp=39 rp=01 smsc=1234567890 destination=5551234 alphabet=0 user_length=1 outcome=0 status_report=0',
            'GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2',
            'GSM service downlink kind=18 sapi=3 pd=09 message=01 length=5',
            'GSM service uplink sapi=3 pd=09 message=04 length=2 data=3904',
            'LAPDm service Channel Release acknowledged',
            'PCH no-identity fill',
        ))
        verify(trace)
        with self.assertRaises(ValueError):
            verify(trace.replace('a70141', 'a70142'))

    def test_requires_physical_input(self):
        with self.assertRaisesRegex(ValueError, 'physical A'):
            verify('')

    def test_rejects_fixture_error(self):
        with self.assertRaisesRegex(ValueError, 'fixture error'):
            verify('[LUA ERROR]')


if __name__ == '__main__':
    unittest.main()
