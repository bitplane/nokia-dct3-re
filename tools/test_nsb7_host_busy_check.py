"""Busy must not become a connected/traffic call; physical recovery is required."""
import unittest
from unittest.mock import patch
from tools.nsb7_host_busy_check import check


def inputs(steps):
    rows=[]
    for second,key in steps:
        code=0x19 if key=='Left Softkey / Menu' else 0x0e if key=='Send' else 0x0f if key=='End' else int(key[-1])
        rows += [f'nsb7_outgoing_call_input: key={key} state=1 t={second}.000000000',
                 f'nsb7_key_decode: key={code:02x} observer_writes=0',
                 f'nsb7_outgoing_call_input: key={key} state=0 t={second}.200000000']
    return rows


def fixture():
    rows=['nsb7_host_outgoing_ready: t=45.000000000']
    rows+=inputs([(46+i,'Keypad '+d) for i,d in enumerate('5551234')]+[(54,'Send')])
    rows += [
        'GSM service establish sapi=0 pd=05 message=24 length=16 data=05247103205881080910101032547698',
        'GSM service downlink kind=5 sapi=0 pd=05 message=21 length=2',
        'GSM service uplink sapi=0 pd=03 message=05 length=18 data=03450404600201805e0581551532f4150101',
        'GSM outgoing request id=1 digits=5551234',
        'gsm_call_adapter: request id=1 epoch=1 digits=5551234 clients=1',
        'gsm_call_adapter: decision id=2 outcome=1 result=rejected',
        'gsm_session: outgoing decision queued id=1 outcome=1',
        'gsm_session: outgoing decision consumed id=1 outcome=1',
        'gsm_call_adapter: decision id=1 outcome=1 result=accepted',
        'gsm_call_adapter: decision id=1 outcome=1 result=rejected',
        'GSM service downlink kind=10 sapi=0 pd=03 message=02 length=2',
        'GSM service downlink kind=13 sapi=0 pd=03 message=25 length=5',
        'GSM service uplink sapi=0 pd=03 message=2d length=2',
        'GSM service downlink kind=26 sapi=0 pd=03 message=2a length=2',
        'LAPDm service Channel Release acknowledged',
        'PCH no-identity fill',
        'gsm_call_adapter: state id=1 epoch=1 phase=ended',
    ]
    rows+=inputs([(66,'End'),(70,'Left Softkey / Menu'),(73,'End')])
    return '\n'.join(rows+['nsb7_outgoing_call_done: t=76.000000000'])


class BusyTest(unittest.TestCase):
    @patch('tools.nsb7_host_busy_check.check_authentication')
    def test_complete(self, authentication):
        check(fixture())
        authentication.assert_called_once_with('')

    @patch('tools.nsb7_host_busy_check.check_authentication')
    def test_connected_or_traffic_is_not_busy(self, authentication):
        for extra in ('GSM service downlink kind=12 sapi=0 pd=03 message=07 length=2',
                      'GSM service downlink kind=14 sapi=0 pd=06 message=2e length=8'):
            with self.subTest(extra=extra),self.assertRaises(ValueError):
                check(fixture()+'\n'+extra)

    @patch('tools.nsb7_host_busy_check.check_authentication')
    def test_physical_recovery_and_epoch_required(self, authentication):
        for old,new in [('key=Left Softkey / Menu state=1 t=70.','key=End state=1 t=70.'),
                        ('epoch=1 phase=ended','epoch=2 phase=ended'),
                        ('key=19 observer_writes=0','key=18 observer_writes=0')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                check(fixture().replace(old,new))

    def test_authentication_required(self):
        with self.assertRaises(ValueError):
            check(fixture())


if __name__=='__main__':
    unittest.main()
