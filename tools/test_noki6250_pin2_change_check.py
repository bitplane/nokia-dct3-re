import unittest
from tools.noki6250_pin_retry_check import registered_card
from tools.noki6250_pin2_change_check import actions,check


def fixture(old='5678',new='8765',*,rejected=False,stored='5678',initial_retries=3):
    lines=[]
    for second,key in actions(old,new):
        lines += [f'6250_pin2_input: key={key} state=1 t={second}.000000000',
                  f'6250_pin2_input: key={key} state=0 t={second}.150000000']
        if second==12:
            lines += ['header cla=a0 ins=20 p1=00 p2=01 p3=08',
                      f'SIM status ins=20 sw=9000 chv=3/{initial_retries} puk=10/10 enabled=1',
                      'LAPDm Location Updating Accept acknowledged']
        if second==70:
            lines += ['header cla=a0 ins=24 p1=00 p2=02 p3=10',
                      f'SIM status ins=24 sw={("9840" if initial_retries==1 else "9804") if rejected else "9000"} chv=3/{initial_retries-1 if rejected else 3} puk=10/10 enabled=1']
    lines.append('6250_pin2_input: complete=1 firmware_state_forcing=0')
    card=bytearray(registered_card())
    card[3492:3500]=(stored if rejected else new).encode('ascii')+b'\xff'*4
    card[3517]=initial_retries-1 if rejected else 3
    return '\n'.join(lines),bytes(card)


class Pin2ChangeTest(unittest.TestCase):
    def test_every_physical_failure_preserves_pin_and_consumes_one_attempt(self):
        for attempts in (3,2,1):
            options=dict(old='8764',new='2468',rejected=True,stored='8765',initial_retries=attempts)
            text,card=fixture(**options)
            with self.subTest(attempts=attempts):
                check(text,card,**options)
            changed=bytearray(card)
            changed[3517]=attempts
            with self.assertRaises(ValueError):
                check(text,bytes(changed),**options)
            if attempts==1:
                with self.assertRaises(ValueError):
                    check(text.replace('ins=24 sw=9840','ins=24 sw=9804'),card,**options)

    def test_wrong_current_pin2_does_not_install_proposed_pin(self):
        options=dict(old='8764',new='2468',rejected=True,stored='8765')
        text,card=fixture(**options)
        check(text,card,**options)
        changed=bytearray(card)
        changed[3492:3500]=b'2468\xff\xff\xff\xff'
        with self.assertRaises(ValueError):
            check(text,bytes(changed),**options)
        with self.assertRaises(ValueError):
            check(text.replace('chv=3/2','chv=3/3'),card,**options)

    def test_cold_success_preserves_initial_retry_loss_until_change(self):
        options=dict(old='8765',new='2468',initial_retries=2)
        text,card=fixture(**options)
        check(text,card,**options)
        with self.assertRaises(ValueError):
            check(text.replace('ins=20 sw=9000 chv=3/2','ins=20 sw=9000 chv=3/3'),card,**options)
        with self.assertRaises(ValueError):
            check(text,card,old='8765',new='2468')

    def test_installed_pin2_can_be_the_cold_current_credential(self):
        text,card=fixture('8765','2468')
        check(text,card,old='8765',new='2468')
        with self.assertRaises(ValueError):
            check(text,card,old='5678',new='2468')

    def test_pin2_values_require_four_ascii_digits(self):
        text,card=fixture()
        for value in ('123','12345','12x4','\uff11\uff12\uff13\uff14'):
            with self.subTest(value=value),self.assertRaises(ValueError):
                check(text,card,new=value)

    def test_physical_change_contract(self):
        check(*fixture())

    def test_selectors_results_and_own_menu_inputs_are_required(self):
        text,card=fixture()
        for old,new in [('p2=02 p3=10','p2=01 p3=10'),('p2=01 p3=08','p2=02 p3=08'),
                        ('ins=24 sw=9000','ins=24 sw=9804'),('chv=3/3','chv=3/2'),
                        ('key=Scroll Down state=1 t=31.','key=Scroll Down state=1 t=32.'),
                        ('key=Keypad 8 state=0 t=58.','key=Keypad 8 state=1 t=58.'),
                        ('complete=1','complete=0')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                check(text.replace(old,new),card)

    def test_change_cannot_precede_confirmation(self):
        text,card=fixture()
        header='header cla=a0 ins=24 p1=00 p2=02 p3=10'
        with self.assertRaises(ValueError):
            check(header+'\n'+text.replace(header,''),card)

    def test_entire_card_and_only_one_change_are_required(self):
        text,card=fixture()
        for offset in (0,1604,3484,3492,3516,3517,len(card)-1):
            changed=bytearray(card)
            changed[offset]^=1
            with self.subTest(offset=offset),self.assertRaises(ValueError):
                check(text,bytes(changed))
        with self.assertRaises(ValueError):
            check(text+'\nSIM status ins=24 sw=9000 chv=3/3 puk=10/10 enabled=1',card)


if __name__=='__main__':unittest.main()
