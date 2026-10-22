import unittest
from pathlib import Path
from unittest.mock import patch
from tools.make_sim_card_profile import make_profile
from tools.noki6250_pin_retry_check import registered_card
from tools.noki6250_pin_request_check import actions,check,check_frames
from tools.sim_security_trace_check import ENABLED_OFFSET


def fixture(*,enable=False,cold=False):
    initial=bytearray(registered_card() if enable or cold else make_profile(True))
    if enable:initial[ENABLED_OFFSET]=0
    final=bytearray(registered_card())
    final[ENABLED_OFFSET]=int(enable or cold)
    lines=[]
    for second,key in actions(enable=enable,cold=cold):
        if second==(28 if cold else 20):lines.append('LAPDm Location Updating Accept acknowledged')
        lines += [f'6250_pin_request: key={key} state=1 t={second}.000000000',
                  f'6250_pin_request: key={key} state=0 t={second}.150000000']
        if second==(18 if cold else 12):
            lines += ['header cla=a0 ins=20 p1=00 p2=01 p3=08',
                      'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1']
        if second==46:
            instruction='28' if enable else '26'
            lines += [f'header cla=a0 ins={instruction} p1=00 p2=01 p3=08',
                      f'SIM status ins={instruction} sw=9000 chv=3/3 puk=10/10 enabled={int(enable)}']
    lines.append('6250_pin_request: complete=1 firmware_state_forcing=0')
    return '\n'.join(lines),bytes(initial),bytes(final)


class PinRequestTest(unittest.TestCase):
    def test_frame_contract_distinguishes_inherited_off_setting_and_restored_prompt(self):
        captures={}
        for enable,cold in ((False,False),(True,False),(False,True)):
            with patch('tools.noki6250_pin_request_check.check_rgb_frame') as renderer:
                check_frames(Path('/capture'),enable=enable,cold=cold)
            captures[enable,cold]={call.args[0].name:call.args[1] for call in renderer.call_args_list}
            self.assertEqual(len(captures[enable,cold]),3 if cold else 11)
        self.assertNotEqual(captures[False,False]['6250_pin_request_34.png'],
                            captures[True,False]['6250_pin_request_34.png'])
        self.assertEqual(captures[False,False]['6250_pin_request_boot.png'],
                         captures[False,True]['6250_pin_request_boot.png'])
        self.assertNotEqual(captures[True,False]['6250_pin_request_boot.png'],
                            captures[False,True]['6250_pin_request_boot.png'])

    def test_disable_cold_enable_and_cold_authentication(self):
        for enable,cold in ((False,False),(True,False),(False,True)):
            with self.subTest(enable=enable,cold=cold):
                check(*fixture(enable=enable,cold=cold),enable=enable,cold=cold)

    def test_only_declared_flag_and_initial_location_transition_are_allowed(self):
        for enable,cold in ((False,False),(True,False),(False,True)):
            text,initial,final=fixture(enable=enable,cold=cold)
            for which in ('initial','final'):
                for offset in (0,1604,3484,3492,3516,3517,3518,3519,ENABLED_OFFSET,
                               len(initial if which=='initial' else final)-1):
                    changed=bytearray(initial if which=='initial' else final)
                    changed[offset]^=1
                    with self.subTest(enable=enable,cold=cold,offset=offset,which=which),self.assertRaises(ValueError):
                        check(text,bytes(changed) if which=='initial' else initial,
                              bytes(changed) if which=='final' else final,enable=enable,cold=cold)

    def test_contacts_selectors_results_and_unrelated_commands_fail_closed(self):
        text,initial,final=fixture()
        for old,new in (('ins=26 p1=00 p2=01','ins=26 p1=00 p2=02'),
                        ('ins=26 sw=9000','ins=26 sw=9804'),
                        ('puk=10/10 enabled=0','puk=10/10 enabled=1'),
                        ('key=Scroll Down state=1 t=44.','key=Scroll Up state=1 t=44.'),
                        ('key=Keypad 4 state=0 t=41.','key=Keypad 4 state=1 t=41.'),
                        ('complete=1','complete=0')):
            with self.subTest(old=old),self.assertRaises(ValueError):
                check(text.replace(old,new),initial,final)
        for ins in ('24','2c','44','dc'):
            with self.subTest(ins=ins),self.assertRaises(ValueError):
                check(text+f'\nheader cla=a0 ins={ins} p1=00 p2=01 p3=08',initial,final)

    def test_enable_cold_boot_must_not_verify_and_setting_must_follow_confirmation(self):
        text,initial,final=fixture(enable=True)
        with self.assertRaises(ValueError):
            check(text+'\nheader cla=a0 ins=20 p1=00 p2=01 p3=08',initial,final,enable=True)
        header='header cla=a0 ins=28 p1=00 p2=01 p3=08'
        with self.assertRaises(ValueError):
            check(header+'\n'+text.replace(header,''),initial,final,enable=True)


if __name__=='__main__':unittest.main()
