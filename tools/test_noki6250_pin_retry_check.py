import unittest
from tools.noki6250_pin_retry_check import check,registered_card


def fixture():
    items=[(second,'Keypad 9') for second in range(8,12)]
    items += [(12,'Left Softkey / Menu')]+[(18+i,'Keypad '+d) for i,d in enumerate('1234')]
    items += [(22,'Left Softkey / Menu'),(34,'Left Softkey / Menu')]
    lines=[]
    for second,key in items:
        lines += [f'6250_pin_retry: key={key} state=1 t={second}.000000000',
                  f'6250_pin_retry: key={key} state=0 t={second}.150000000']
        if second in (12,22):
            lines.append('header cla=a0 ins=20 p1=00 p2=01 p3=08')
            lines.append('SIM status ins=20 sw=9804 chv=2/3 puk=10/10 enabled=1' if second==12 else
                         'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1')
        if second==22:
            lines.append('LAPDm Location Updating Accept acknowledged')
    lines.append('6250_pin_retry: complete=1 firmware_state_forcing=0')
    return '\n'.join(lines),registered_card()


class PinRetryTest(unittest.TestCase):
    def test_physical_wrong_pin_then_recovery(self):
        check(*fixture())

    def test_selector_retry_counter_and_physical_inputs_are_required(self):
        text,card=fixture()
        for old,new in [('p2=01','p2=02'),('sw=9804','sw=9000'),
                        ('chv=2/3','chv=1/3'),('chv=3/3','chv=2/3'),
                        ('key=Keypad 9 state=1','key=Keypad 8 state=1'),
                        ('key=Keypad 4 state=0','key=Keypad 4 state=1'),
                        ('complete=1','complete=0')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                check(text.replace(old,new),card)

    def test_registration_cannot_precede_correct_pin(self):
        text,card=fixture()
        line='LAPDm Location Updating Accept acknowledged'
        with self.assertRaises(ValueError):
            check(line+'\n'+text.replace(line,''),card)

    def test_entire_card_is_compared(self):
        text,card=fixture()
        for offset in (0,1604,3484,3516,3517,len(card)-1):
            changed=bytearray(card)
            changed[offset]^=1
            with self.subTest(offset=offset),self.assertRaises(ValueError):
                check(text,bytes(changed))


if __name__=='__main__':unittest.main()
