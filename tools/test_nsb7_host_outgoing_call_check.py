"""Host decision correlation composes with the actual outgoing lifecycle checker."""
import re
import unittest
from unittest.mock import patch
from tools.nsb7_host_outgoing_call_check import check
from tools.test_nsb7_outgoing_call_check import fixture as call_fixture


def fixture():
    text=re.sub(r't=(\d+)\.',lambda m:f't={int(m[1])+26}.',call_fixture())
    def decode(match):
        key=match[1]
        code=0x0e if key=='Send' else 0x0f if key=='End' else int(key[-1])
        return match[0]+f'\nnsb7_key_decode: key={code:02x} observer_writes=0'
    text=re.sub(r'nsb7_outgoing_call_input: key=(.*?) state=1 t=[0-9.]+',decode,text)
    text='nsb7_host_outgoing_ready: t=45.000000000\n'+text
    proceeding='GSM service downlink kind=10 sapi=0 pd=03 message=02 length=2'
    decisions='\n'.join([
        'gsm_call_adapter: request id=1 epoch=1 digits=5551234 clients=1',
        'gsm_call_adapter: decision id=2 outcome=1 result=rejected',
        'outgoing decision queued id=1 outcome=0',
        'outgoing decision consumed id=1 outcome=0',
        'gsm_call_adapter: decision id=1 outcome=0 result=accepted',
        'gsm_call_adapter: decision id=1 outcome=0 result=rejected',
    ])
    text=text.replace(proceeding,decisions+'\n'+proceeding)
    ack='GSM service uplink sapi=0 pd=03 message=0f length=2'
    text=text.replace(ack,ack+'\ngsm_call_adapter: state id=1 epoch=1 phase=connected')
    release='GSM service uplink sapi=0 pd=03 message=2a length=2'
    text=text.replace(release,release+'\ngsm_call_adapter: state id=1 epoch=1 phase=media_closed\n'
                      'gsm_call_adapter: state id=1 epoch=1 phase=ended')
    return text


class HostOutgoingTest(unittest.TestCase):
    @patch('tools.nsb7_host_outgoing_call_check.check_authentication')
    def test_composed_lifecycle(self, authentication):
        text=fixture()
        check(text)
        authentication.assert_called_once_with('')

    @patch('tools.nsb7_host_outgoing_call_check.check_authentication')
    def test_correlation_and_once_only(self, authentication):
        for text in (fixture().replace('epoch=1 phase=connected','epoch=2 phase=connected'),
                     fixture()+'\noutgoing decision queued id=1 outcome=0',
                     fixture().replace('key=0e observer_writes=0','key=0f observer_writes=0'),
                     fixture().replace('id=2 outcome=1 result=rejected','id=2 outcome=1 result=accepted')):
            with self.subTest(text=text[-80:]),self.assertRaises(ValueError):
                check(text)

    def test_authentication_required(self):
        with self.assertRaises(ValueError):
            check(fixture())


if __name__=='__main__':
    unittest.main()
