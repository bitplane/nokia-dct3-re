import unittest
from pathlib import Path
from unittest.mock import patch
from tools.make_sim_card_profile import make_profile
from tools.noki6250_pin_retry_check import registered_card
from tools.noki6250_pin_change_check import actions,check,check_frames


def fixture():
    card=bytearray(registered_card())
    card[3484:3492]=b'2468\xff\xff\xff\xff'
    lines=[]
    for second,key in actions():
        lines += [f'6250_pin_change: key={key} state=1 t={second}.000000000',
                  f'6250_pin_change: key={key} state=0 t={second}.150000000']
        if second==12:
            lines += ['header cla=a0 ins=20 p1=00 p2=01 p3=08',
                      'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1',
                      'LAPDm Location Updating Accept acknowledged']
        if second==70:
            lines += ['header cla=a0 ins=24 p1=00 p2=01 p3=10',
                      'SIM status ins=24 sw=9000 chv=3/3 puk=10/10 enabled=1']
    lines.append('6250_pin_change: complete=1 firmware_state_forcing=0')
    return '\n'.join(lines),make_profile(True),bytes(card)


class OrdinaryPinChangeTest(unittest.TestCase):
    def test_eleven_own_route_editor_result_and_recovery_frames(self):
        with patch('tools.noki6250_pin_change_check.check_rgb_frame') as renderer:
            check_frames(Path('/capture'))
        frames={int(call.args[0].stem.rsplit('_',1)[1]):call.args[1] for call in renderer.call_args_list}
        self.assertEqual(set(frames),{26,31,34,39,44,46,54,62,70,76,78})
        self.assertEqual(frames[70],'41dc6e07aad8c509e4767b9dd3f75f1b3fbca86c9f0927b2473cdfb4ad8e2dea')

    def test_ordinary_change_is_chv1_not_unblock_or_chv2(self):
        check(*fixture())
        text,initial,final=fixture()
        for old,new in (('ins=24 p1=00 p2=01','ins=24 p1=00 p2=02'),
                        ('ins=24 p1=00','ins=2c p1=00'),
                        ('ins=24 sw=9000','ins=24 sw=9804'),
                        ('chv=3/3','chv=2/3'),('puk=10/10','puk=9/10')):
            with self.subTest(old=old),self.assertRaises(ValueError):
                check(text.replace(old,new),initial,final)

    def test_complete_cards_preserve_other_credentials_counters_and_records(self):
        text,initial,final=fixture()
        for which in ('initial','final'):
            source=initial if which=='initial' else final
            for offset in (0,1604,3484,3492,3500,3516,3517,3518,3519,3520,len(source)-1):
                changed=bytearray(source)
                changed[offset]^=1
                with self.subTest(which=which,offset=offset),self.assertRaises(ValueError):
                    check(text,bytes(changed) if which=='initial' else initial,
                          bytes(changed) if which=='final' else final)

    def test_command_follows_confirmation_and_all_contacts_are_required(self):
        text,initial,final=fixture()
        header='header cla=a0 ins=24 p1=00 p2=01 p3=10'
        for mutated in (header+'\n'+text.replace(header,''),
                        text.replace('key=Keypad 8 state=0 t=69.','key=Keypad 8 state=1 t=69.'),
                        text.replace('complete=1','complete=0'),
                        text+'\nheader cla=a0 ins=26 p1=00 p2=01 p3=08'):
            with self.subTest(mutated=mutated[-100:]),self.assertRaises(ValueError):
                check(mutated,initial,final)


if __name__=='__main__':unittest.main()
