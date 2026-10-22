import unittest
from tools.noki6210_host_ussd_check import check_notification
from tools.noki6210_puk_check import profile


def fixture():
    lines=[]
    items=[(8+i,'Keypad '+d) for i,d in enumerate('1234')]
    items += [(12,'Left Softkey / Menu'),(26,'Right Softkey / C'),(30,'Left Softkey / Menu')]
    for second,key in items:
        if second==26:
            lines += [
                'gsm_call_adapter: incoming ussd id=1 result=accepted t=20.010000',
                'gsm_call_adapter: incoming ussd state id=1 epoch=1 phase=queued',
                'gsm_ss: network_initiated operation=notify transaction=0b invoke=1 dcs=0f',
                'GSM service downlink kind=32 sapi=0 pd=0b message=3b length=29',
                'GSM service uplink sapi=0 pd=0b message=3a length=8 data=8b3a05a203020101',
                'GSM service downlink kind=27 sapi=0 pd=0b message=2a length=2',
                'LAPDm service Channel Release acknowledged nr=6',
                'gsm_call_adapter: incoming ussd state id=1 epoch=1 phase=delivered',
            ]
        lines += [f'6210_host_ussd_input: key={key} state=1 t={second}.000000000',
                  f'6210_host_ussd_input: key={key} state=0 t={second}.150000000']
        if second==12:
            lines += ['SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1',
                      'gsm_call_adapter: network registered=1 arfcn=35']
    lines.append('6210_host_ussd: complete=1 firmware_state_forcing=0')
    return '\n'.join(lines),profile(3,True)


class HostNotificationTests(unittest.TestCase):
    def test_notification_and_physical_recovery(self):
        check_notification(*fixture())

    def test_wrong_epoch_message_operation_input_or_order_fails(self):
        text,card=fixture()
        for old,new in (('epoch=1 phase=delivered','epoch=2 phase=delivered'),
                        ('phase=delivered','phase=queued'),('operation=notify','operation=request'),
                        ('arfcn=35','arfcn=1'),('t=20.010000','t=19.010000'),
                        ('length=29','length=28'),('data=8b3a05a203020101','data=8b3a05a203020102'),
                        ('key=Right Softkey / C','key=End'),('complete=1','complete=0')):
            with self.subTest(old=old),self.assertRaises(ValueError):
                check_notification(text.replace(old,new),card)

    def test_duplicate_admission_or_notification_fails(self):
        text,card=fixture()
        for line in ('gsm_call_adapter: incoming ussd id=1 result=accepted t=20.010000',
                     'gsm_ss: network_initiated operation=notify transaction=0b invoke=1 dcs=0f'):
            with self.subTest(line=line),self.assertRaises(ValueError):
                check_notification(text+'\n'+line,card)

    def test_whole_card_and_no_credential_or_record_writes(self):
        text,card=fixture()
        for index in (0,1604,3484,3516,3520,3569):
            changed=bytearray(card)
            changed[index]^=1
            with self.subTest(index=index),self.assertRaises(ValueError):
                check_notification(text,bytes(changed))
        for ins in (4,0x24,0x26,0x28,0x2c,0x44,0xdc):
            with self.subTest(ins=ins),self.assertRaises(ValueError):
                check_notification(text+f'\nSIM status ins={ins:02x} sw=9000 chv=3/3 puk=10/10 enabled=1',card)


if __name__=='__main__':
    unittest.main()
