"""Regression cases for NSB-7 physical outgoing SMS acceptance."""
import unittest
import re

from tools.nsb7_outgoing_sms_check import check


def fixture():
    steps = [(28, 'Left Softkey / Menu'), (30, 'Scroll Down'),
             (32, 'Left Softkey / Menu'), (34, 'Left Softkey / Menu'),
             (36, 'Keypad 2'), (38, 'Left Softkey / Menu'),
             (40, 'Left Softkey / Menu'), (42, 'Left Softkey / Menu')]
    lines = [f'nsb7_sms_reply_input: key={key} state={state} t={second}.{fraction}'
             for second, key in steps
             for state, fraction in [('1', '000000000'), ('0', '200000000')]]
    lines += ['GSM service uplink sapi=3 pd=09 message=01 length=5 data=8901020240',
              'data=390118000100069121436587090d11000781551532f40000a70141',
              'gsm_sms_submit: cp=39 rp=01 smsc=1234567890 destination=5551234 '
              'alphabet=0 user_length=1 outcome=0 status_report=0',
              'GSM service downlink kind=17 sapi=3 pd=09 message=04',
              'GSM service downlink kind=18 sapi=3 pd=09 message=01',
              'GSM service uplink sapi=3 pd=09 message=04 length=2 data=3904',
              'LAPDm service Channel Release acknowledged nr=2',
              'nsb7_sms_reply_done: t=70.000000000']
    return '\n'.join(lines)


class OutgoingSmsTest(unittest.TestCase):
    def test_explicit_physical_delay(self):
        text = re.sub(r't=(\d+)\.', lambda m: f't={int(m[1])+4}.', fixture())
        check(text, delay=4)
        with self.assertRaises(ValueError):
            check(text)

    def test_complete(self):
        check(fixture())

    def test_rejects_missing_or_corrupt_evidence(self):
        for token in ['Keypad 2', 'destination=5551234', 'a70141', 'kind=17',
                      'kind=18', 'data=3904', 'acknowledged nr=2', 't=70.']:
            with self.subTest(token=token), self.assertRaises(ValueError):
                check(fixture().replace(token, 'incorrect'))

    def test_rejects_duplicate_incoming(self):
        with self.assertRaises(ValueError):
            check(fixture() + '\nGSM service uplink sapi=3 pd=09 message=01 length=5 data=8901020240')

    def test_rejects_duplicate_submit(self):
        with self.assertRaises(ValueError):
            check(fixture() + '\ngsm_sms_submit: duplicate')


if __name__ == '__main__':
    unittest.main()
