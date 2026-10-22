import re
import unittest
from unittest.mock import patch
from tools.test_nsb7_outgoing_sms_check import fixture as reply_fixture
from tools.nsb7_host_sms_failure_check import check


def fixture(silent=False):
    from tools.test_radio_outgoing_host_sms_trace_check import HostSmsTraceCheckTest
    text=re.sub(r't=(\d+)\.',lambda m:f't={int(m[1])+4}.',reply_fixture())
    rows=[]
    for row in text.splitlines():
        if 'GSM service downlink kind=18' in row: continue
        rows.append(row)
        match=re.match(r'nsb7_sms_reply_input: key=(.*?) state=1 ',row)
        if match:
            code='18' if match[1]=='Scroll Down' else '02' if match[1]=='Keypad 2' else '19'
            rows.append(f'nsb7_key_decode: key={code} observer_writes=0')
    host=HostSmsTraceCheckTest().failure_log('rp_silence' if silent else 'rp_error')
    host=host.replace('octets=2','octets=1').replace('phase=ended',
        'phase=rejected\ngsm_call_adapter: sms state id=1 epoch=1 phase=ended')
    rows.append(host)
    delay=90 if silent else 0
    for time,key,code in ((78,'End','0f'),(82,'Left Softkey / Menu','19'),(85,'End','0f')):
        rows += [f'nsb7_sms_recovery: key={key} state=1 t={time+delay}.000000000',
                 f'nsb7_key_decode: key={code} observer_writes=0',
                 f'nsb7_sms_recovery: key={key} state=0 t={time+delay}.200000000']
    return '\n'.join(rows+[f'nsb7_sms_recovery: done t={88+delay}.000000000'])


class FailedSmsTest(unittest.TestCase):
    @patch('tools.nsb7_host_sms_failure_check.check_authentication')
    def test_distinct_complete_failures(self,authentication):
        for silent in (False,True):
            with self.subTest(silent=silent): check(fixture(silent),silent=silent)

    @patch('tools.nsb7_host_sms_failure_check.check_authentication')
    def test_failure_scope_physical_and_host_corruption_rejected(self,authentication):
        for silent in (False,True):
            text=fixture(silent)
            code=3 if silent else 1
            for bad in (text.replace('key=02 observer_writes=0','key=03 observer_writes=0'),
                        text.replace('epoch=1 phase=ended','epoch=2 phase=ended'),
                        text.replace('nsb7_sms_recovery: done','missing done'),
                        text+f'\ngsm_call_adapter: sms decision id=1 outcome={code} result=accepted',
                        text+'\nGSM service downlink kind=18 sapi=3',
                        text.replace('destination=5551234','destination=5551235')):
                with self.subTest(silent=silent,bad=bad),self.assertRaises(ValueError):
                    check(bad,silent=silent)

    def test_authentication_required(self):
        with self.assertRaises(ValueError): check(fixture())
