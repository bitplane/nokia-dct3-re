import unittest
from tools.noki6250_pin_retry_check import registered_card
from tools.noki6250_pin2_change_check import actions
from tools.noki6250_puk2_check import check


def fixture():
    initial=bytearray(registered_card())
    initial[3492:3500]=b'8765\xff\xff\xff\xff'
    initial[3517]=0
    final=bytearray(initial)
    final[3492:3500]=b'2468\xff\xff\xff\xff'
    final[3517]=3
    items=[(second,key) for second,key in actions() if second<50]
    for start,digits in ((50,'87654321'),(62,'2468'),(70,'2468')):
        items += [(start+i,'Keypad '+digit) for i,digit in enumerate(digits)]+[(start+len(digits),'Left Softkey / Menu')]
    items += [(80,'End'),(82,'Left Softkey / Menu')]
    lines=[]
    for second,key in items:
        lines += [f'6250_pin2_input: key={key} state=1 t={second}.000000000',
                  f'6250_pin2_input: key={key} state=0 t={second}.150000000']
        if second==12:
            lines += ['header cla=a0 ins=20 p1=00 p2=01 p3=08',
                      'SIM status ins=20 sw=9000 chv=3/0 puk=10/10 enabled=1',
                      'LAPDm Location Updating Accept acknowledged']
        if second==74:
            lines += ['header cla=a0 ins=2c p1=00 p2=02 p3=10',
                      'SIM status ins=2c sw=9000 chv=3/3 puk=10/10 enabled=1',
                      'header cla=a0 ins=20 p1=00 p2=02 p3=08',
                      'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1']
    lines.append('6250_pin2_input: complete=1 firmware_state_forcing=0')
    return '\n'.join(lines),bytes(initial),bytes(final)


class Puk2RecoveryTest(unittest.TestCase):
    def test_unblock_and_followup_new_pin_verification(self):
        check(*fixture())

    def test_selector_result_counter_and_contact_mutations_fail(self):
        text,initial,final=fixture()
        for old,new in [('ins=2c p1=00 p2=02','ins=2c p1=00 p2=00'),
                        ('ins=20 p1=00 p2=02','ins=20 p1=00 p2=01'),
                        ('ins=2c sw=9000','ins=2c sw=9804'),('chv=3/0','chv=3/1'),
                        ('key=Keypad 8 state=1 t=50.','key=Keypad 9 state=1 t=50.'),
                        ('key=Keypad 8 state=0 t=73.','key=Keypad 8 state=1 t=73.'),
                        ('complete=1','complete=0')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                check(text.replace(old,new),initial,final)

    def test_complete_predecessor_and_recovered_cards_are_required(self):
        text,initial,final=fixture()
        for offset in (0,1604,3484,3492,3516,3517,3518,3519,len(final)-1):
            for which in ('initial','final'):
                changed=bytearray(initial if which=='initial' else final)
                changed[offset]^=1
                with self.subTest(offset=offset,which=which),self.assertRaises(ValueError):
                    check(text,bytes(changed) if which=='initial' else initial,
                          bytes(changed) if which=='final' else final)

    def test_unblock_must_follow_physical_confirmation(self):
        text,initial,final=fixture()
        header='header cla=a0 ins=2c p1=00 p2=02 p3=10'
        with self.assertRaises(ValueError):
            check(header+'\n'+text.replace(header,''),initial,final)


if __name__=='__main__':unittest.main()
