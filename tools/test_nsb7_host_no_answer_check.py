import unittest
from unittest.mock import patch
from tools.test_nsb7_host_busy_check import fixture as busy_fixture
from tools.nsb7_host_no_answer_check import check


def fixture():
    text=busy_fixture()
    start=text.index('GSM service downlink kind=13')
    end=text.index('nsb7_outgoing_call_input: key=End state=1 t=66.')
    text=text[:start]+text[end:]
    text=text.replace('outcome=1','outcome=2').replace(
        'decision id=2 outcome=2','decision id=2 outcome=1')
    alert='\n'.join((
        'GSM service downlink kind=14 sapi=0 pd=06 message=2e length=8',
        'GSM service uplink sapi=0 pd=06 message=29 length=3 data=062900',
        'GSM service downlink kind=11 sapi=0 pd=03 message=01 length=2',
        'gsm_call_adapter: state id=1 epoch=1 phase=alerting media_uplink_sequence=0 media_downlink_sequence=0'))
    text=text.replace('nsb7_outgoing_call_input: key=End state=1 t=66.',
        alert+'\nnsb7_outgoing_call_input: key=End state=1 t=66.',1)
    release='\n'.join((
        'GSM service uplink sapi=0 pd=03 message=25 length=5 data=032502e090',
        'GSM service downlink kind=19 sapi=0 pd=03 message=2d length=2',
        'GSM service uplink sapi=0 pd=03 message=2a length=2 data=036a',
        'LAPDm service Channel Release acknowledged nr=3','PCH no-identity fill channel=60',
        'gsm_call_adapter: state id=1 epoch=1 phase=ended'))
    return text.replace('nsb7_outgoing_call_input: key=Left Softkey / Menu state=1 t=70.',
        release+'\nnsb7_outgoing_call_input: key=Left Softkey / Menu state=1 t=70.',1)


class NoAnswerTest(unittest.TestCase):
    @patch('tools.nsb7_host_busy_check.check_authentication')
    def test_complete(self,authentication):
        check(fixture())
        authentication.assert_called_once_with('')

    @patch('tools.nsb7_host_busy_check.check_authentication')
    def test_connected_media_wrong_epoch_and_missing_recovery_rejected(self,authentication):
        text=fixture()
        for bad in (text.replace('phase=alerting','phase=connected'),
                    text.replace('media_uplink_sequence=0','media_uplink_sequence=1'),
                    text.replace('epoch=1 phase=ended','epoch=2 phase=ended'),
                    text.replace('key=Left Softkey / Menu state=1 t=70.','key=End state=1 t=70.'),
                    text+'\nGSM service downlink kind=12 sapi=0 pd=03 message=07 length=2',
                    text.replace('decision id=1 outcome=2 result=accepted',
                                 'decision id=1 outcome=1 result=accepted')):
            with self.subTest(bad=bad),self.assertRaises(ValueError):
                check(bad)

    def test_authentication_required(self):
        with self.assertRaises(ValueError):
            check(fixture())
