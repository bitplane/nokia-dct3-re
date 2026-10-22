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


if __name__=='__main__':unittest.main()
