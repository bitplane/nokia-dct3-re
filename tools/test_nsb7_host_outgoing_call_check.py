"""Host decision correlation composes with the actual outgoing lifecycle checker."""
import re
import unittest
from unittest.mock import patch
from tools.nsb7_host_outgoing_call_check import check,check_clock
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
    def test_continuous_clock_despite_analog_writes(self):
        text=''.join(f'ccont_rtc: event=second time=13:{(47*60+45+i)//60:02d}:{(45+i)%60:02d} day=0 t={i}.000000000\n'
                     for i in range(1,77))
        text+='ccont_analog: event=control_write reg=07 data=00\n'
        text+='ccont_analog: event=control_write reg=07 data=0f\n'
        check_clock(text)
        for invalid in (text.replace('time=13:48:00','time=13:47:00'),
                        text.replace('data=0f','data=01'),
                        text.replace('t=76.000000000','t=77.000000000')):
            with self.assertRaises(ValueError):
                check_clock(invalid)

    @patch('tools.nsb7_host_outgoing_call_check.check_authentication')
    def test_adapter_ack_can_precede_emulation_consumption(self, authentication):
        consumed='outgoing decision consumed id=1 outcome=0'
        duplicate='gsm_call_adapter: decision id=1 outcome=0 result=rejected'
        text=fixture().replace(consumed+'\n','').replace(duplicate,duplicate+'\n'+consumed)
        check(text)
        queued='outgoing decision queued id=1 outcome=0'
        invalid=text.replace(consumed+'\n','').replace(queued,consumed+'\n'+queued)
        with self.assertRaises(ValueError):
            check(invalid)

    @patch('tools.nsb7_host_outgoing_call_check.check_authentication')
    def test_restored_epoch_and_stale_decision(self, authentication):
        text=fixture().replace('epoch=1 phase=media_closed','epoch=2 phase=media_closed').replace(
            'epoch=1 phase=ended','epoch=2 phase=ended')
        end='nsb7_outgoing_call_input: key=End state=1 t=66.000000000'
        restored='\n'.join(['nsb7_state: event=saved','nsb7_state: event=restored',
            'gsm_call_adapter: state id=1 epoch=2 phase=connected',
            'gsm_call_adapter: stale epoch=1 current=2 type=decision id=1 result=rejected'])
        text=text.replace(end,restored+'\n'+end)
        check(text,restored=True)
        with self.assertRaises(ValueError):
            check(text)
        with self.assertRaises(ValueError):
            check(text.replace('epoch=2 phase=connected','epoch=1 phase=connected'),restored=True)

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
