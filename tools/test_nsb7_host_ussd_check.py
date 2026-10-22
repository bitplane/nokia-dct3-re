import unittest
from unittest.mock import patch
from tools import nsb7_host_ussd_check as check


TRACE='''gsm_call_adapter: network registered=1 arfcn=600
gsm_call_adapter: incoming ussd id=1 result=accepted t=24.010000
incoming ussd state id=1 epoch=1 phase=queued
gsm_ss: network_initiated operation=notify transaction=0b invoke=1 dcs=0f
GSM service downlink kind=32 sapi=0 pd=0b message=3b length=29
GSM service uplink sapi=0 pd=0b message=3a length=8 data=8b3a05a203020101
GSM service downlink kind=27 sapi=0 pd=0b message=2a length=2
LAPDm service Channel Release acknowledged
incoming ussd state id=1 epoch=1 phase=delivered
nsb7_ussd_input: key=Left Softkey / Menu state=1 t=30.000000000
nsb7_ussd_input: key=End state=1 t=32.000000000
nsb7_ussd_input: key=Left Softkey / Menu state=1 t=34.000000000
nsb7_ussd: complete
'''


class UssdNotificationTest(unittest.TestCase):
    def verify(self,text=TRACE,final=b'card'):
        with patch.object(check,'check_source'),patch.object(check,'check_authentication'):
            check.check(text,b'card',final)

    def test_ordered_physical_notification(self):
        self.verify()

    def test_requires_settled_admission_and_acknowledgement(self):
        for old,new in [('t=24.010000','t=13.300000'),
                        ('data=8b3a05a203020101','data=8b3a05a203020102'),
                        ('arfcn=600','arfcn=1'),('nsb7_ussd: complete','')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                self.verify(TRACE.replace(old,new))

    def test_duplicate_and_wrong_epoch_fail(self):
        for text in (TRACE+TRACE,TRACE.replace('epoch=1 phase=delivered','epoch=2 phase=delivered')):
            with self.assertRaises(ValueError):self.verify(text)

    def test_card_mutation_and_write_commands_fail(self):
        with self.assertRaises(ValueError):self.verify(final=b'changed')
        with self.assertRaises(ValueError):self.verify(TRACE+'sim_device: update fid=6f3c\n')

    def test_physical_read_and_exit_are_required(self):
        for marker in ('key=End state=1','key=Left Softkey / Menu state=1 t=30.'):
            with self.subTest(marker=marker),self.assertRaises(ValueError):
                self.verify(TRACE.replace(marker,'missing'))


class UssdRequestTest(unittest.TestCase):
    def trace(self):
        text='gsm_call_adapter: network registered=1 arfcn=600\n'
        for second,key in ((26,'Keypad *'),(27,'Keypad 1'),(28,'Keypad 2'),
                           (29,'Keypad 3'),(30,'Keypad #'),(32,'Send')):
            text+=self.key(second,key)
        text+='''GSM service uplink sapi=0 pd=0b message=3b length=27 data=1b7b1c14a11202010102013b300a04010f0405aa986c36027f0100
gsm_ss: request=ussd_host id=1 dcs=0f packed_length=5
gsm_call_adapter: ussd request id=1 epoch=1 dcs=0f packed_length=5
gsm_call_adapter: ussd response id=2 outcome=0 result=rejected
gsm_call_adapter: ussd response id=1 outcome=0 result=accepted
gsm_call_adapter: ussd state id=1 epoch=1 phase=accepted
GSM service downlink kind=27 sapi=0 pd=0b message=2a length=32
LAPDm service Channel Release acknowledged
gsm_call_adapter: ussd state id=1 epoch=1 phase=ended
'''
        return text+self.key(38,'End')+self.key(40,'Left Softkey / Menu')+'nsb7_ussd: complete\n'

    def key(self,second,key):
        return (f'nsb7_ussd_input: key={key} state=1 t={second}.000000000\n'
                f'nsb7_ussd_input: key={key} state=0 t={second}.200000000\n')

    def verify(self,text,final=b'card'):
        with patch.object(check,'check_source'),patch.object(check,'check_authentication'):
            check.check_request(text,b'card',final)

    def test_physical_request_and_host_response(self):
        self.verify(self.trace())

    def test_missing_rejection_or_wrong_payload_fail(self):
        for old,new in [('result=rejected','result=accepted'),
                        ('aa986c3602','aa986c3603'),
                        ('key=Keypad # state=0','key=Keypad * state=0'),
                        ('epoch=1 phase=ended','epoch=2 phase=ended')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                self.verify(self.trace().replace(old,new))

    def test_storage_or_missing_recovery_fail(self):
        with self.assertRaises(ValueError):self.verify(self.trace(),b'changed')
        with self.assertRaises(ValueError):
            self.verify(self.trace().replace('key=End state=1','key=Send state=1'))

    def restored_trace(self):
        cpu=','.join(['00000000']*37)
        def snapshot(event):
            return f'nsb7_state: event={event} pc=002f8300 sp=00130df8 ram=567b1438 cpu={cpu} t=33.800000000\n'
        needle='gsm_call_adapter: ussd request id=1 epoch=1 dcs=0f packed_length=5\n'
        replacement=(needle+snapshot('saved')+snapshot('restored')+
            'state_roundtrip: result=pass scenario=nsb7_ussd requested_at=33.800000000 t=33.800000000\n'+
            needle.replace('epoch=1','epoch=2')+
            'gsm_call_adapter: ussd response id=1 outcome=0 result=rejected\n')
        return self.trace().replace(needle,replacement).replace('epoch=1 phase=','epoch=2 phase=')

    def verify_restored(self,text):
        with patch.object(check,'check_source'),patch.object(check,'check_authentication'), \
             patch.object(check,'check_clock'):
            check.check_request(text,b'card',b'card',restored=True)

    def test_pending_restore_is_architectural_not_equal_traffic_replay(self):
        self.verify_restored(self.restored_trace())

    def test_restore_requires_exact_architecture_and_stale_rejection(self):
        text=self.restored_trace()
        for bad in (text.replace('event=restored pc=002f8300','event=restored pc=002f8302'),
                    text.replace('ussd response id=1 outcome=0 result=rejected','ussd response id=1 outcome=0 result=accepted'),
                    text.replace('ussd request id=1 epoch=2','ussd request id=2 epoch=2')):
            with self.assertRaises(ValueError):self.verify_restored(bad)


if __name__=='__main__':unittest.main()
