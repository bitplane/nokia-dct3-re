import unittest
from unittest.mock import patch

from tools.nsb7_ussd_continued_check import KEYS,check


def fixture():
    lines=[]
    def keys(items):
        for second,key in items:
            lines.extend([f'nsb7_ussd_input: key={key} state=1 t={second}.000000000',
                          f'nsb7_ussd_input: key={key} state=0 t={second}.200000000'])
    keys(KEYS[:6])
    lines.extend([
        'GSM service uplink sapi=0 pd=0b message=3b length=27 data=1b7b1c14a11202010102013b300a04010f0405aa986c36027f0100 t=33',
        'gsm_ss: request=ussd transaction=1b invoke=1 dcs=0f packed_length=5 outcome=4 t=33',
        'GSM service downlink kind=31 sapi=0 pd=0b message=3a length=28 t=33',
    ])
    keys(KEYS[6:9])
    lines.extend([
        'gsm_ss: continued handset_response message=3a length=21 data=1b3a12a210020102300b02013c300604010f040132 t=40',
        'gsm_ss: continued_result transaction=1b invoke=2 dcs=0f packed_length=1 empty=0 t=40',
        'GSM service downlink kind=27 sapi=0 pd=0b message=2a length=37 t=40',
        'radio_phase=release_channel_change',
    ])
    keys(KEYS[9:])
    lines.append('nsb7_ussd: complete')
    return '\n'.join(lines)


class ContinuedUssdTests(unittest.TestCase):
    def verify(self,text,final=b'card'):
        with patch('tools.nsb7_ussd_continued_check.check_source'),patch('tools.nsb7_ussd_continued_check.check_authentication'):
            check(text,b'card',final)

    def test_physical_correlated_reply(self):
        self.verify(fixture())

    def test_bad_framing_correlations_or_input_fail(self):
        original=fixture()
        for old,new in [('length=28','length=29'),('transaction=1b invoke=2','transaction=1b invoke=3'),
                        ('packed_length=1','packed_length=2'),('t=40.000000000','t=41.000000000'),
                        ('empty=0','empty=1'),('nsb7_ussd: complete','incomplete')]:
            self.assertIn(old,original)
            with self.subTest(old=old),self.assertRaises(ValueError):self.verify(original.replace(old,new))

    def test_storage_change_and_missing_release_fail(self):
        with self.assertRaises(ValueError):self.verify(fixture(),b'changed')
        with self.assertRaises(ValueError):self.verify(fixture().replace('radio_phase=release_channel_change',''))


if __name__=='__main__':unittest.main()
