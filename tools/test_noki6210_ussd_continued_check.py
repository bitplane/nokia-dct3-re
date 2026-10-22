import unittest
from tools.noki6210_puk_check import profile
from tools.noki6210_ussd_continued_check import check


def fixture():
    lines=[]
    actions=[(8+i,'Keypad '+d) for i,d in enumerate('1234')]
    actions += [(12,'Left Softkey / Menu'),(26,'Keypad *'),(27,'Keypad 1'),
                (28,'Keypad 2'),(29,'Keypad 3'),(30,'Keypad #'),(31,'Send'),
                (36,'Left Softkey / Menu'),(40,'Keypad 2'),
                (42,'Left Softkey / Menu'),(46,'End'),(48,'Left Softkey / Menu')]
    for second,key in actions:
        lines += [f'6210_host_ussd_input: key={key} state=1 t={second}.000000000',
                  f'6210_host_ussd_input: key={key} state=0 t={second}.150000000']
        if second==12:
            lines += ['header cla=a0 ins=20 p1=00 p2=01 p3=08',
                      'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1',
                      'LAPDm Location Updating Accept acknowledged']
        if second==31:
            lines += ['GSM service uplink pd=0b message=3b length=27 data=1b7b1c14a11202010102013b300a04010f0405aa986c36027f0100 ',
                      'gsm_ss: request=ussd transaction=1b invoke=1 dcs=0f packed_length=5 outcome=4 ',
                      'GSM service downlink kind=32 sapi=0 pd=0b message=3a length=28 ']
        if second==42:
            lines += ['gsm_ss: continued handset_response message=3a length=21 data=1b3a12a210020102300b02013c300604010f040132 ',
                      'gsm_ss: continued_result transaction=1b invoke=2 dcs=0f packed_length=1 empty=0 ',
                      'GSM service downlink kind=27 sapi=0 pd=0b message=2a length=37 ',
                      'radio_phase=release_channel_change ',
                      'LAPDm service Channel Release acknowledged']
    lines.append('6210_host_ussd: complete=1 firmware_state_forcing=0')
    return '\n'.join(lines),profile(3,True)


class ContinuedUssdTest(unittest.TestCase):
    def test_physical_continued_reply(self):
        check(*fixture())

    def test_mutated_protocol_input_and_lifecycle_are_rejected(self):
        text,card=fixture()
        for old,new in [('outcome=4','outcome=0'),('invoke=2','invoke=3'),
                        ('040132','040133'),('length=37','length=12'),
                        ('key=Keypad 2 state=1 t=40.','key=Keypad 3 state=1 t=40.'),
                        ('radio_phase=release_channel_change','radio_phase=other'),
                        ('p2=01 p3=08','p2=02 p3=08')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                check(text.replace(old,new),card)
        with self.assertRaises(ValueError):
            check(text,card[:-1]+bytes([card[-1]^1]))

    def test_duplicate_transaction_is_rejected(self):
        text,card=fixture()
        with self.assertRaises(ValueError):
            check(text+'\ngsm_ss: request=ussd ',card)


if __name__=='__main__':unittest.main()
