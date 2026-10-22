import unittest
from pathlib import Path
from unittest.mock import patch
from tools.noki6250_pin_retry_check import registered_card
from tools.noki6250_pin2_change_check import actions
from tools.noki6250_puk2_check import check,check_permanent,check_frames


def fixture(*,rejected=False,puk_attempts=10):
    initial=bytearray(registered_card())
    initial[3492:3500]=b'8765\xff\xff\xff\xff'
    initial[3517]=0
    initial[3519]=puk_attempts
    final=bytearray(initial)
    if rejected:
        final[3519]-=1
    else:
        final[3492:3500]=b'2468\xff\xff\xff\xff'
        final[3517]=3
        final[3519]=10
    items=[(second,key) for second,key in actions() if second<50]
    for start,digits in ((50,'99999999' if rejected else '87654321'),(62,'2468'),(70,'2468')):
        items += [(start+i,'Keypad '+digit) for i,digit in enumerate(digits)]+[(start+len(digits),'Left Softkey / Menu')]
    items += [(80,'End'),(82,'Left Softkey / Menu')]
    lines=[]
    for second,key in items:
        lines += [f'6250_pin2_input: key={key} state=1 t={second}.000000000',
                  f'6250_pin2_input: key={key} state=0 t={second}.150000000']
        if second==12:
            lines += ['header cla=a0 ins=20 p1=00 p2=01 p3=08',
                      f'SIM status ins=20 sw=9000 chv=3/0 puk=10/{puk_attempts} enabled=1',
                      'LAPDm Location Updating Accept acknowledged']
        if second==74:
            lines += ['header cla=a0 ins=2c p1=00 p2=02 p3=10',
                      f'SIM status ins=2c sw={"9840" if rejected and puk_attempts==1 else "9804" if rejected else "9000"} chv=3/{0 if rejected else 3} puk=10/{puk_attempts-1 if rejected else 10} enabled=1']
            if not rejected:
                lines += ['header cla=a0 ins=20 p1=00 p2=02 p3=08',
                          'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1']
    lines.append('6250_pin2_input: complete=1 firmware_state_forcing=0')
    return '\n'.join(lines),bytes(initial),bytes(final)


class Puk2RecoveryTest(unittest.TestCase):
    def test_last_rejection_has_its_own_error_and_post_end_frames(self):
        with patch('tools.noki6250_puk2_check.check_rgb_frame') as renderer:
            check_frames(Path('/capture'),rejected=True,puk_attempts=1)
        frames={int(call.args[0].stem.rsplit('_',1)[1]):call.args[1] for call in renderer.call_args_list}
        self.assertEqual(len(frames),11)
        self.assertEqual(frames[74],'6357f9b5bc04027166bc3529fa4abd4ca429af4c953db90b97e6697f44984152')
        self.assertEqual(frames[80],frames[82])
        self.assertEqual(frames[80],'6e9133cb23ceaf7021771d33e2e49fa09dcf41c76ebac0f18c767078ae9e0d9f')

    def test_permanent_pin2_loss_keeps_pin1_and_rejects_any_new_credential_request(self):
        card=bytearray(registered_card())
        card[3492:3500]=b'8765\xff\xff\xff\xff'
        card[3517]=card[3519]=0
        items=[(second,key) for second,key in actions() if second<50]
        items += [(50,'End'),(52,'Left Softkey / Menu')]
        lines=[]
        for second,key in items:
            lines += [f'6250_pin2_input: key={key} state=1 t={second}.000000000',
                      f'6250_pin2_input: key={key} state=0 t={second}.150000000']
            if second==12:
                lines += ['header cla=a0 ins=20 p1=00 p2=01 p3=08',
                          'SIM status ins=20 sw=9000 chv=3/0 puk=10/0 enabled=1',
                          'LAPDm Location Updating Accept acknowledged']
        text='\n'.join(lines+['6250_pin2_input: complete=1 firmware_state_forcing=0'])
        check_permanent(text,bytes(card),bytes(card))
        for suffix in ('header cla=a0 ins=2c p1=00 p2=02 p3=10',
                       'header cla=a0 ins=20 p1=00 p2=02 p3=08'):
            with self.subTest(suffix=suffix),self.assertRaises(ValueError):
                check_permanent(text+'\n'+suffix,bytes(card),bytes(card))
        for offset in (3484,3492,3516,3517,3518,3519,len(card)-1):
            changed=bytearray(card)
            changed[offset]^=1
            with self.subTest(offset=offset),self.assertRaises(ValueError):
                check_permanent(text,bytes(card),bytes(changed))

    def test_every_live_puk2_allowance_and_last_attempt_status(self):
        for attempts in range(1,11):
            with self.subTest(attempts=attempts):
                check(*fixture(rejected=True,puk_attempts=attempts),rejected=True,puk_attempts=attempts)
                check(*fixture(puk_attempts=attempts),puk_attempts=attempts)
        text,initial,final=fixture(rejected=True,puk_attempts=1)
        with self.assertRaises(ValueError):
            check(text.replace('ins=2c sw=9840','ins=2c sw=9804'),initial,final,rejected=True,puk_attempts=1)
        for invalid in (0,11,-1,True,'1'):
            with self.subTest(invalid=invalid),self.assertRaises(ValueError):
                check(text,initial,final,rejected=True,puk_attempts=invalid)

    def test_wrong_puk2_decrements_only_its_allowance(self):
        text,initial,final=fixture(rejected=True)
        check(text,initial,final,rejected=True)
        changed=bytearray(final)
        changed[3492:3500]=b'2468\xff\xff\xff\xff'
        with self.assertRaises(ValueError):
            check(text,initial,bytes(changed),rejected=True)
        with self.assertRaises(ValueError):
            check(text.replace('puk=10/9','puk=10/10'),initial,final,rejected=True)
        with self.assertRaises(ValueError):
            check(text+'\nheader cla=a0 ins=20 p1=00 p2=02 p3=08',initial,final,rejected=True)

    def test_cold_correct_puk2_requires_retained_nine_attempts(self):
        text,initial,final=fixture(puk_attempts=9)
        check(text,initial,final,puk_attempts=9)
        with self.assertRaises(ValueError):
            check(text,initial,final)
        changed=bytearray(initial)
        changed[3519]=10
        with self.assertRaises(ValueError):
            check(text,bytes(changed),final,puk_attempts=9)

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
