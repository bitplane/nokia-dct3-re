import unittest
from pathlib import Path
from unittest.mock import patch
from tools.make_sim_card_profile import make_profile
from tools.sim_fdn_profile import extend
from tools.noki6250_pin_retry_check import registered_card
from tools.noki6250_fdn_check import actions,check,check_frames
from tools.nsk3_phonebook_check import expected_record


def fixture(phase,*,rejected=False,pin2_attempts=3,preserved=False):
    initial=bytearray(extend(make_profile(True)+bytes.fromhex('00f110')+b'\xff'*43) if phase==0 and not preserved else extend(registered_card()))
    initial[3575]=int(phase in (0,3))
    initial[3517]=pin2_attempts
    final=bytearray(extend(registered_card()))
    final[3575]=int(phase>=2 or rejected)
    if phase%2:final[3517]=pin2_attempts
    if rejected:final[3517]=pin2_attempts-1
    lines=[]
    def command(ins,fid):
        lines.extend([f'header cla=a0 ins={ins:02x} p1=00 p2=00 p3=00 selected={fid:04x}',
                      f'SIM status ins={ins:02x} sw=9000 chv=3/3 puk=10/10 enabled=1'])
    for second,key in actions(phase,rejected=rejected):
        lines += [f'6250_fdn: key={key} state=1 t={second}.000000000',
                  f'6250_fdn: key={key} state=0 t={second}.150000000']
        if second==12:
            lines += ['header cla=a0 ins=20 p1=00 p2=01 p3=08',
                      f'SIM status ins=20 sw=9000 chv=3/{pin2_attempts} puk=10/10 enabled=1']
            if phase in (1,2):
                for fid in (0x6f7e,0x6f07):command(0x44,fid)
            lines += [f'header cla=a0 ins=b2 p1={index:02x} p2=04 p3=20 selected=6f3b' for index in range(1,11)]
            lines.append('LAPDm Location Updating Accept acknowledged')
        if second==51:
            lines += ['header cla=a0 ins=20 p1=00 p2=02 p3=08',
                      f'SIM status ins=20 sw={"9804" if rejected else "9000"} chv=3/{pin2_attempts-1 if rejected else 3} puk=10/10 enabled=1']
            if not rejected:command(4 if phase==0 else 0x44,0x6f3a)
            if phase==0 and not rejected:
                for fid in (0x6f7e,0x6f07):command(0x44,fid)
    lines.append('6250_fdn: complete=1 firmware_state_forcing=0')
    return '\n'.join(lines),bytes(initial),bytes(final)


class FdnLifecycleTest(unittest.TestCase):
    def test_physical_off_preserves_a_saved_fdn_record(self):
        text,initial,final=fixture(2)
        before,after=bytearray(initial),bytearray(final)
        before[3578:3610]=after[3578:3610]=expected_record()
        check(text,bytes(before),bytes(after),2,preserved=True,record=expected_record())
        with self.assertRaises(ValueError):check(text,bytes(before),bytes(after),2,preserved=True)
        after[3578:3610]=b'\xff'*32
        with self.assertRaises(ValueError):
            check(text,bytes(before),bytes(after),2,preserved=True,record=expected_record())

    def test_rejected_activation_cannot_invalidate_adn_or_repair_pin2_attempts(self):
        text,initial,final=fixture(0,rejected=True)
        check(text,initial,final,0,rejected=True)
        self.assertEqual(final[3517],2)
        self.assertEqual(final[3575],1)
        with self.assertRaises(ValueError):
            check(text+'\nheader cla=a0 ins=04 p1=00 p2=00 p3=00 selected=6f3a',initial,final,0,rejected=True)
        repaired=bytearray(final)
        repaired[3517]=3
        with self.assertRaises(ValueError):check(text,initial,bytes(repaired),0,rejected=True)
        for phase in (1,2,3):
            with self.subTest(phase=phase),self.assertRaises(ValueError):actions(phase,rejected=True)

    def test_cold_activation_requires_the_full_registered_two_attempt_card(self):
        values=fixture(0,pin2_attempts=2,preserved=True)
        check(*values,0,pin2_attempts=2,preserved=True)
        with self.assertRaises(ValueError):check(*values,0,preserved=True)
        with self.assertRaises(ValueError):check(*values,0,pin2_attempts=2)

    def test_cold_observation_never_repairs_pin2_without_verification(self):
        text,initial,final=fixture(1,pin2_attempts=2)
        check(text,initial,final,1,pin2_attempts=2)
        repaired=bytearray(final)
        repaired[3517]=3
        with self.assertRaises(ValueError):check(text,initial,bytes(repaired),1,pin2_attempts=2)

    def test_all_four_phases(self):
        for phase in range(4):
            with self.subTest(phase=phase):check(*fixture(phase),phase)
        for phase in (-1,4,True):
            with self.subTest(phase=phase),self.assertRaises(ValueError):actions(phase)

    def test_full_card_comparison_preserves_credentials_records_and_retry_allowances(self):
        for phase in range(4):
            text,initial,final=fixture(phase)
            for which in ('initial','final'):
                source=initial if which=='initial' else final
                for offset in (0,1604,3484,3492,3516,3517,3518,3519,3520,3575,3578,len(source)-1):
                    changed=bytearray(source)
                    changed[offset]^=1
                    with self.subTest(phase=phase,which=which,offset=offset),self.assertRaises(ValueError):
                        check(text,bytes(changed) if which=='initial' else initial,
                              bytes(changed) if which=='final' else final,phase)

    def test_selectors_file_targets_read_extent_and_physical_contacts(self):
        text,initial,final=fixture(0)
        for old,new in (('ins=20 p1=00 p2=02','ins=20 p1=00 p2=01'),
                        ('ins=04 p1=00 p2=00 p3=00 selected=6f3a','ins=04 p1=00 p2=00 p3=00 selected=6f3b'),
                        ('selected=6f7e','selected=6f07'),('ins=04 sw=9000','ins=04 sw=9804'),
                        ('ins=b2 p1=0a','ins=b2 p1=09'),
                        ('key=Keypad 8 state=0 t=50.','key=Keypad 8 state=1 t=50.'),
                        ('complete=1','complete=0')):
            with self.subTest(old=old),self.assertRaises(ValueError):
                check(text.replace(old,new),initial,final,0)
        for ins in ('24','26','28','2c','dc'):
            with self.subTest(ins=ins),self.assertRaises(ValueError):
                check(text+f'\nheader cla=a0 ins={ins} p1=00 p2=01 p3=08',initial,final,0)

    def test_verification_and_validity_follow_physical_confirmation(self):
        text,initial,final=fixture(2)
        header='header cla=a0 ins=20 p1=00 p2=02 p3=08'
        with self.assertRaises(ValueError):
            check(header+'\n'+text.replace(header,''),initial,final,2)

    def test_frames_pin_both_cold_states_and_toggle_results(self):
        frames=[]
        for phase in range(4):
            with patch('tools.noki6250_fdn_check.check_rgb_frame') as renderer:
                check_frames(Path('/capture'),phase)
            frames.append({call.args[0].name:call.args[1] for call in renderer.call_args_list})
        self.assertEqual([len(item) for item in frames],[9,6,9,6])
        self.assertEqual(frames[0]['6250_fdn_40.png'],frames[3]['6250_fdn_40.png'])
        self.assertEqual(frames[1]['6250_fdn_40.png'],frames[2]['6250_fdn_40.png'])
        self.assertNotEqual(frames[0]['6250_fdn_51.png'],frames[2]['6250_fdn_51.png'])


if __name__=='__main__':unittest.main()
