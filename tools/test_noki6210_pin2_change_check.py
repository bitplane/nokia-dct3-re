import unittest

from tools.noki6210_pin2_change_check import check,check_puk2,settings_contacts,check_permanent_puk2
from tools.noki6210_puk_check import profile
from tools.sim_security_trace_check import statuses


def fixture():
    items=[(7+i,'Keypad '+str(i)) for i in range(1,5)]+[(12,'Left Softkey / Menu')]
    items += [(20,'Left Softkey / Menu')]+[(22+i,'Scroll Down') for i in range(3)]+[(26,'Left Softkey / Menu')]
    items += [(28+i,'Scroll Down') for i in range(5)]+[(34,'Left Softkey / Menu')]
    items += [(36+i,'Scroll Down') for i in range(4)]+[(42,'Left Softkey / Menu')]
    items += [(44+i,'Scroll Down') for i in range(2)]+[(47,'Left Softkey / Menu')]
    for start,digits in ((50,'5678'),(58,'8765'),(66,'8765')):
        items.extend((start+i,'Keypad '+d) for i,d in enumerate(digits))
        items.append((start+4,'Left Softkey / Menu'))
    items += [(76,'End'),(78,'Left Softkey / Menu')]
    lines=[]
    for second,key in items:
        lines.extend([f'6210_pin2_input: key={key} state=1 t={second}.000000000',
                      f'6210_pin2_input: key={key} state=0 t={second}.150000000'])
        if second==12:
            lines.extend(['SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1',
                          'LAPDm Location Updating Accept acknowledged nr=1'])
        if second==70:
            lines.extend(['sim_device: header cla=a0 ins=24 p1=00 p2=02 p3=10',
                          'SIM status ins=24 sw=9000 chv=3/3 puk=10/10 enabled=1'])
    lines.append('6210_pin2_input: complete=1 firmware_state_forcing=0')
    card=bytearray(profile(3,True))
    card[3492:3500]=b'8765\xff\xff\xff\xff'
    return '\n'.join(lines),bytes(card)


class Pin2ChangeTests(unittest.TestCase):
    def puk2_fixture(self):
        items=settings_contacts()
        for start,digits in ((50,'87654321'),(62,'2468'),(70,'2468')):
            items.extend((start+i,'Keypad '+d) for i,d in enumerate(digits))
            items.append((start+len(digits),'Left Softkey / Menu'))
        items += [(80,'End'),(82,'Left Softkey / Menu')]
        lines=[]
        for second,key in items:
            lines.extend([f'6210_pin2_input: key={key} state=1 t={second}.000000000',
                          f'6210_pin2_input: key={key} state=0 t={second}.150000000'])
            if second==12:
                lines.extend(['header cla=a0 ins=20 p1=00 p2=01 p3=08',
                              'SIM status ins=20 sw=9000 chv=3/0 puk=10/10 enabled=1',
                              'LAPDm Location Updating Accept acknowledged nr=1'])
            if second==74:
                lines.extend(['header cla=a0 ins=2c p1=00 p2=02 p3=10',
                              'SIM status ins=2c sw=9000 chv=3/3 puk=10/10 enabled=1',
                              'header cla=a0 ins=20 p1=00 p2=02 p3=08',
                              'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1'])
        lines.append('6210_pin2_input: complete=1 firmware_state_forcing=0')
        initial=bytearray(profile(3,True))
        initial[3517]=0
        final=bytearray(profile(3,True))
        final[3492:3500]=b'2468\xff\xff\xff\xff'
        return '\n'.join(lines),bytes(initial),bytes(final)

    def test_physical_puk2_recovery(self):
        check_puk2(*self.puk2_fixture())

    def test_permanent_puk2_control_keeps_pin1_but_has_no_pin2_or_service_transaction(self):
        text,initial,_=self.puk2_fixture()
        text='\n'.join(line for line in text.splitlines() if
                       not ('ins=2c' in line or 'p2=02' in line or 'sw=9000 chv=3/3' in line))
        text=text.replace('chv=3/0 puk=10/10','chv=3/0 puk=10/0')
        card=bytearray(initial)
        card[3519]=0
        card=bytes(card)
        check_permanent_puk2(text,card,card)
        for extra in ('header cla=a0 ins=2c p1=00 p2=02 p3=10',
                      'header cla=a0 ins=20 p1=00 p2=02 p3=08',
                      'GSM service establish sapi=0 pd=03 message=05 ',
                      'GSM service uplink sapi=3 pd=09 message=01 ',
                      'GSM service establish sapi=0 pd=05 message=24 '):
            with self.subTest(extra=extra),self.assertRaises(ValueError):
                check_permanent_puk2(text+'\n'+extra,card,card)
        for index in (3492,3516,3517,3518,3519):
            changed=bytearray(card)
            changed[index]^=1
            with self.assertRaises(ValueError):
                check_permanent_puk2(text,card,bytes(changed))

    def test_wrong_puk2_is_durable_and_does_not_replace_credentials(self):
        text,initial,_=self.puk2_fixture()
        for second,digit in enumerate('87654321',50):
            for state in (0,1):
                text=text.replace(f'key=Keypad {digit} state={state} t={second}.',
                                  f'key=Keypad 9 state={state} t={second}.')
        text=text.replace('ins=2c sw=9000 chv=3/3 puk=10/10','ins=2c sw=9804 chv=3/0 puk=10/9')
        text=text.replace('header cla=a0 ins=20 p1=00 p2=02 p3=08\n','')
        text=text.replace('SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1\n','')
        final=bytearray(initial)
        final[3519]=9
        check_puk2(text,initial,bytes(final),rejected=True)
        last_initial=bytearray(initial)
        last_initial[3519]=1
        permanent=bytearray(last_initial)
        permanent[3519]=0
        terminal=text.replace('sw=9804 chv=3/0 puk=10/9','sw=9840 chv=3/0 puk=10/0')
        terminal=terminal.replace('sw=9000 chv=3/0 puk=10/10','sw=9000 chv=3/0 puk=10/1')
        check_puk2(terminal,bytes(last_initial),bytes(permanent),rejected=True,puk_attempts=1)
        with self.assertRaisesRegex(ValueError,'must not VERIFY'):
            check_puk2(text+'\nheader cla=a0 ins=20 p1=00 p2=02 p3=08',
                       initial,bytes(final),rejected=True)
        for index in (3492,3517,3518,3519):
            changed=bytearray(final)
            changed[index]^=1
            with self.assertRaises(ValueError):
                check_puk2(text,initial,bytes(changed),rejected=True)

    def test_correct_puk2_recovers_after_a_persisted_failure(self):
        text,initial,final=self.puk2_fixture()
        initial=bytearray(initial)
        initial[3519]=9
        text=text.replace('chv=3/0 puk=10/10','chv=3/0 puk=10/9')
        check_puk2(text,bytes(initial),final,puk_attempts=9)
        with self.assertRaises(ValueError):
            check_puk2(text,bytes(initial),final)

    def test_puk2_selector_status_input_and_other_credentials_are_required(self):
        text,initial,final=self.puk2_fixture()
        for old,new in (('p2=02','p2=01'),('sw=9000 chv=3/3','sw=9804 chv=3/3'),
                        ('t=74.000000000','t=75.000000000'),('key=Keypad 8','key=Keypad 9')):
            with self.subTest(old=old),self.assertRaises(ValueError):
                check_puk2(text.replace(old,new),initial,final)
        for index in (3484,3492,3508,3516,3517,3519):
            changed=bytearray(final)
            changed[index]^=1
            with self.assertRaises(ValueError):
                check_puk2(text,initial,bytes(changed))
        with self.assertRaises(ValueError):
            check_puk2(text,final,final)
        with self.assertRaises(ValueError):
            check_puk2(text.replace('header cla=a0 ins=20 p1=00 p2=02 p3=08','missing'),initial,final)

    def test_status_selector_keeps_both_credential_namespaces_distinct(self):
        text='SIM status ins=24 sw=9804 chv=3/2 puk=9/8 enabled=1'
        self.assertEqual(statuses(text,0x24),[(0x9804,3,9)])
        self.assertEqual(statuses(text,0x24,2),[(0x9804,2,8)])
        with self.assertRaises(ValueError):
            statuses(text,0x24,0)

    def test_wrong_pin2_changes_only_its_retry_counter(self):
        # Only the current-code phase changes; replacement and PIN1 stay intact.
        text,_=fixture()
        for time,digit in enumerate('5678',50):
            text=text.replace(f'key=Keypad {digit} state=1 t={time}.',f'key=Keypad 9 state=1 t={time}.')
            text=text.replace(f'key=Keypad {digit} state=0 t={time}.',f'key=Keypad 9 state=0 t={time}.')
        text=text.replace('ins=24 sw=9000 chv=3/3','ins=24 sw=9804 chv=3/2')
        card=bytearray(profile(3,True))
        card[3517]=2
        check(text,bytes(card),old_pin='9999',rejected=True)
        self.assertEqual(statuses(text,0x24),[(0x9804,3,10)])
        self.assertEqual(statuses(text,0x24,2),[(0x9804,2,10)])
        for remaining in (1,0):
            exhausted=bytearray(card)
            exhausted[3517]=remaining
            result='9840' if remaining==0 else '9804'
            transition=text.replace('ins=24 sw=9804 chv=3/2',
                                    f'ins=24 sw={result} chv=3/{remaining}')
            check(transition,bytes(exhausted),old_pin='9999',rejected=True,remaining=remaining)
            with self.assertRaises(ValueError):
                check(transition,bytes(card),old_pin='9999',rejected=True,remaining=remaining)
        for index in (3492,3516,3517,3519):
            changed=bytearray(card)
            changed[index]^=1
            with self.assertRaises(ValueError):
                check(text,bytes(changed),old_pin='9999',rejected=True)

    def test_cold_installed_pin2_is_used_to_change_back(self):
        text,_=fixture()
        # Change the actual keyed old/new phases, not the rest of the workflow.
        import re
        def rotate(match):
            digit,state,second,fraction=match.groups()
            time=int(second)
            digits='8765' if 50<=time<=53 else '5678' if 58<=time<=61 or 66<=time<=69 else None
            if digits is None:
                return match[0]
            start=50 if time<=53 else 58 if time<=61 else 66
            return f'key=Keypad {digits[time-start]} state={state} t={second}.{fraction}'
        text=re.sub(r'key=Keypad (\d) state=([01]) t=(\d+)\.(\d+)',rotate,text)
        card=profile(3,True)
        check(text,card,'8765','5678')
        with self.assertRaises(ValueError):
            check(text,card)

    def test_physical_change(self):
        check(*fixture())

    def test_invalid_retry_count_is_not_a_card_transition(self):
        text,card=fixture()
        for remaining in (-1,3):
            with self.assertRaisesRegex(ValueError,'remaining attempts'):
                check(text,card,rejected=True,remaining=remaining)

    def test_selector_input_or_status_drift_fails(self):
        text,card=fixture()
        for old,new in [('p2=02','p2=01'),('ins=24 sw=9000','ins=24 sw=9804'),
                        ('t=70.000000000','t=71.000000000'),('key=Keypad 8','key=Keypad 9'),
                        ('complete=1','complete=0')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                check(text.replace(old,new),card)

    def test_complete_card_preserves_other_credentials(self):
        text,original=fixture()
        for index in (0,1604,3484,3492,3500,3516,3517,3518,3520,3569):
            card=bytearray(original)
            card[index]^=1
            with self.subTest(index=index),self.assertRaises(ValueError):
                check(text,bytes(card))


if __name__=='__main__':
    unittest.main()
