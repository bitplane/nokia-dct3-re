import unittest
from unittest.mock import patch

from tools.nsb7_network_ussd_request_check import KEYS, check


def fixture():
    lines = [
        'PCH IMSI page transmitted channel=60 fn=1',
        'GSM service establish sapi=0 pd=06 message=27 length=16',
        'gsm_ss: network_initiated operation=request transaction=0b invoke=1 dcs=0f',
        'GSM service downlink kind=32 sapi=0 pd=0b message=3b length=29 t=22',
    ]
    for second, key in KEYS:
        lines.extend([f'nsb7_ussd_input: key={key} state=1 t={second}.000000000',
                      f'nsb7_ussd_input: key={key} state=0 t={second}.200000000'])
        if second == 32:
            lines.extend([
                'gsm_ss: network_initiated handset_response message=3a length=21 data=8b3a12a210020101300b02013c300604010f040132 t=32.2',
                'gsm_ss: network_result transaction=8b invoke=1 dcs=0f packed_length=1 empty=0 t=32.2',
                'GSM service downlink kind=27 sapi=0 pd=0b message=2a length=2 t=32.3',
                'LAPDm service Channel Release acknowledged',
            ])
    lines.append('nsb7_ussd: complete')
    return '\n'.join(lines)


class NetworkUssdTests(unittest.TestCase):
    def verify(self, text, final=b'card'):
        with patch('tools.nsb7_network_ussd_request_check.check_source'), \
                patch('tools.nsb7_network_ussd_request_check.check_authentication'):
            check(text, b'card', final)

    def test_physical_reply(self):
        self.verify(fixture())

    def test_mutations_fail(self):
        original = fixture()
        for old, new in [('length=29', 'length=32'), ('data=8b3a', 'data=1b3a'),
                         ('transaction=8b invoke=1', 'transaction=8b invoke=2'),
                         ('packed_length=1', 'packed_length=2'),
                         ('t=32.000000000', 't=33.000000000'),
                         ('LAPDm service Channel Release acknowledged', 'missing'),
                         ('nsb7_ussd: complete', 'missing')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                self.verify(original.replace(old, new))

    def test_card_change_and_duplicate_fail(self):
        with self.assertRaises(ValueError):
            self.verify(fixture(), b'changed')
        with self.assertRaises(ValueError):
            self.verify(fixture() + '\ngsm_ss: network_result duplicate')


if __name__ == '__main__':
    unittest.main()
