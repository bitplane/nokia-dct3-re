"""Host incoming identity/order and physical-input rejection controls."""
import unittest
from unittest.mock import patch
from tools.nsb7_host_incoming_call_check import check


def fixture():
    return '\n'.join([
        'gsm_call_adapter: network registered=1 arfcn=600',
        'nsb7_call_ready: t=45.000000000',
        'gsm_call_adapter: incoming id=1 result=accepted',
        'gsm_call_adapter: incoming state id=1 epoch=1 phase=queued',
        'gsm_call_adapter: incoming state id=1 epoch=1 phase=paging',
        'GSM service downlink kind=9 sapi=0 pd=03 message=05 length=19',
        'nsb7_lapdm_downlink: pc=002e49e6 task=4 selector=80 block=03244d030504046002008134015c07814477000910322b2b t=46.4',
        'nsb7_lapdm_downlink: pc=002645ec task=14 selector=80 block=03244d030504046002008134015c07814477000910322b2b t=46.5',
        'GSM service uplink sapi=0 pd=03 message=08 length=11 data=8308040460020180150101',
        'GSM service uplink sapi=0 pd=03 message=01 length=2 data=8341',
        'gsm_call_adapter: incoming state id=1 epoch=1 phase=alerting',
        'nsb7_call_input: key=Send state=1 t=52.000000000',
        'nsb7_key_decode: key=0e observer_writes=0',
        'GSM service uplink sapi=0 pd=03 message=07 length=2 data=8307',
        'GSM service downlink kind=20 sapi=0 pd=03 message=0f length=2',
        'gsm_call_adapter: incoming state id=1 epoch=1 phase=connected',
        'nsb7_call_input: key=Send state=0 t=52.200000000',
        'nsb7_call_input: key=End state=1 t=60.000000000',
        'nsb7_key_decode: key=0f observer_writes=0',
        'GSM service uplink sapi=0 pd=03 message=25 length=5 data=836502e090',
        'GSM service downlink kind=25 sapi=0 pd=03 message=2d length=2',
        'GSM service uplink sapi=0 pd=03 message=2a length=2 data=832a',
        'gsm_call_adapter: incoming state id=1 epoch=1 phase=media_closed',
        'gsm_call_adapter: incoming state id=1 epoch=1 phase=ended',
        'nsb7_call_input: key=End state=0 t=60.200000000',
        'nsb7_radio_tx: index=91 type=02 length=20 data=0412020200000016600002580000000001297000',
        'nsb7_radio_tx: index=92 type=57 length=4 data=03050000',
    ])


class HostIncomingTest(unittest.TestCase):
    @patch('tools.nsb7_host_incoming_call_check.check_authentication')
    def test_caller_bytes_reach_both_consumers(self,authentication):
        text=fixture().replace('length=19','length=17').replace(
            '4d030504046002008134015c0781447700091032',
            '45030504046002008134015c0581551532f4')
        check(text,caller='5551234')
        with self.assertRaises(ValueError): check(text)
        for bad in (text.replace('81551532f4','81551532f5',1),
                    text.replace('pc=002645ec','pc=002645ee'),
                    text.replace('length=17','length=19')):
            with self.subTest(bad=bad),self.assertRaises(ValueError):
                check(bad,caller='5551234')
    @patch('tools.nsb7_host_incoming_call_check.check_authentication')
    def test_restored_epoch_rejects_stale_termination(self, authentication):
        text=fixture().replace('epoch=1 phase=media_closed','epoch=2 phase=media_closed').replace(
            'epoch=1 phase=ended','epoch=2 phase=ended')
        end='nsb7_call_input: key=End state=1 t=60.000000000'
        restored='\n'.join(['nsb7_state: event=saved','nsb7_state: event=restored',
            'gsm_call_adapter: incoming state id=1 epoch=2 phase=connected',
            'gsm_call_adapter: stale epoch=1 current=2 type=termination id=1 result=rejected'])
        text=text.replace(end,restored+'\n'+end)
        check(text,restored=True)
        with self.assertRaises(ValueError):
            check(text)
        with self.assertRaises(ValueError):
            check(text.replace('type=termination id=1 result=rejected','type=termination id=1 result=accepted'),restored=True)

    @patch('tools.nsb7_host_incoming_call_check.check_authentication')
    def test_complete_and_authentication_scope(self, authentication):
        text=fixture()
        check(text)
        authentication.assert_called_once_with(text.split('nsb7_call_ready:',1)[0])

    @patch('tools.nsb7_host_incoming_call_check.check_authentication')
    def test_missing_or_wrong_boundary(self, authentication):
        for old,new in [('epoch=1 phase=connected','epoch=2 phase=connected'),
                        ('t=45.000000000','t=25.000000000'),
                        ('key=0e observer_writes=0','key=0f observer_writes=0'),
                        ('data=8307','data=8308'),('type=57 length=4','missing'),
                        ('phase=ended','phase=connected')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                check(fixture().replace(old,new))

    def test_real_authentication_required(self):
        with self.assertRaises(ValueError):
            check(fixture())


if __name__=='__main__':
    unittest.main()
