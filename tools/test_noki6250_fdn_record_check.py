import unittest
from pathlib import Path
from unittest.mock import patch
from tools.noki6250_fdn_check import initial_card
from tools.noki6250_fdn_record_check import actions,check,check_frames,expected_card


def fixture(mode,pin2_attempts=3):
    initial=bytearray(initial_card(1))
    record=bytearray(b'\xff'*32)
    record[0]=ord('A')
    record[18:23]=bytes.fromhex('04812143f5')
    initial[3517]=pin2_attempts
    if mode not in ('write','empty','wrong'):initial[3578:3610]=record
    if mode in ('edited-read','cancel','erase'):initial[3600]=0xf6
    final=bytearray(initial)
    if mode=='write':final[3578:3610]=record
    if mode=='edit':final[3600]=0xf6
    if mode=='erase':final[3578:3610]=b'\xff'*32
    if mode=='wrong':final[3517]-=1
    elif mode=='write':final[3517]=3
    lines=[]
    for second,key in actions(mode):
        lines += [f'6250_fdn: key={key} state=1 t={second}.000000000',
                  f'6250_fdn: key={key} state=0 t={second}.150000000']
        if second==12:
            lines += ['header cla=a0 ins=20 p1=00 p2=01 p3=08',
                      f'SIM status ins=20 sw=9000 chv=3/{pin2_attempts} puk=10/10 enabled=1']
            for fid in ('6f7e','6f07'):
                lines += [f'header cla=a0 ins=44 p1=00 p2=00 p3=00 selected={fid}',
                          f'SIM status ins=44 sw=9000 chv=3/{pin2_attempts} puk=10/10 enabled=1']
            lines += [f'header cla=a0 ins=b2 p1={index:02x} p2=04 p3=20 selected=6f3b' for index in range(1,11)]
            lines.append('LAPDm Location Updating Accept acknowledged')
        if mode in ('write','edit','cancel','erase','wrong') and second=={'write':57,'edit':61,'cancel':62,'erase':62,'wrong':57}[mode]:
            lines += ['header cla=a0 ins=20 p1=00 p2=02 p3=08',
                      f'SIM status ins=20 sw={"9804" if mode=="wrong" else "9000"} chv=3/{pin2_attempts-1 if mode=="wrong" else 3} puk=10/10 enabled=1']
        if mode in ('write','edit','erase') and second=={'write':71,'edit':73,'erase':65}[mode]:
            lines += ['header cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3b',
                      'SIM status ins=dc sw=9000 chv=3/3 puk=10/10 enabled=1']
            if mode!='erase':lines.append('header cla=a0 ins=b2 p1=01 p2=04 p3=20 selected=6f3b')
    lines.append('6250_fdn: complete=1 firmware_state_forcing=0')
    return '\n'.join(lines),bytes(initial),bytes(final)


class ProtectedFdnRecordTest(unittest.TestCase):
    def test_save_and_cold_details_preserve_the_exact_card(self):
        for mode in ('write','read','edit','edited-read','cancel','erase','empty','wrong'):
            with self.subTest(mode=mode):check(*fixture(mode),mode)
        self.assertEqual(expected_card(True),fixture('write')[2])
        for mode in ('bad','unknown'):
            with self.subTest(mode=mode),self.assertRaises(ValueError):actions(mode)

    def test_credentials_validity_other_records_and_required_contact_are_preserved(self):
        for mode in ('write','read','edit','edited-read','cancel','erase','empty','wrong'):
            text,initial,final=fixture(mode)
            for which in ('initial','final'):
                source=initial if which=='initial' else final
                for offset in (0,1604,3484,3492,3516,3517,3518,3519,3520,3575,3578,3598,3610,len(source)-1):
                    changed=bytearray(source)
                    changed[offset]^=1
                    with self.subTest(mode=mode,which=which,offset=offset),self.assertRaises(ValueError):
                        check(text,bytes(changed) if which=='initial' else initial,
                              bytes(changed) if which=='final' else final,mode)

    def test_selectors_update_target_readback_and_order_fail_closed(self):
        text,initial,final=fixture('write')
        for old,new in (('ins=20 p1=00 p2=02','ins=20 p1=00 p2=01'),
                        ('ins=dc p1=01','ins=dc p1=02'),
                        ('ins=dc p1=01 p2=04 p3=20 selected=6f3b','ins=dc p1=01 p2=04 p3=20 selected=6f3a'),
                        ('ins=dc sw=9000','ins=dc sw=9804'),
                        ('ins=b2 p1=0a','ins=b2 p1=09'),
                        ('key=Keypad 8 state=0 t=56.','key=Keypad 8 state=1 t=56.'),
                        ('complete=1','complete=0')):
            with self.subTest(old=old),self.assertRaises(ValueError):
                check(text.replace(old,new),initial,final,'write')
        header='header cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3b'
        with self.assertRaises(ValueError):
            check(header+'\n'+text.replace(header,''),initial,final,'write')
        text,initial,final=fixture('read')
        with self.assertRaises(ValueError):
            check(text+'\n'+header,initial,final,'read')

    def test_own_saved_and_cold_number_frames(self):
        for mode,count in (('write',7),('read',5),('edit',7),('edited-read',5),('cancel',6),('erase',6),('empty',4),('wrong',5)):
            with patch('tools.noki6250_fdn_record_check.check_rgb_frame') as renderer:
                check_frames(Path('/capture'),mode)
            self.assertEqual(len(renderer.call_args_list),count)

    def test_cancel_never_updates_and_confirmed_erase_writes_only_blank_record(self):
        text,initial,final=fixture('cancel')
        self.assertEqual(initial,final)
        for mutation in (text.replace('key=Right Softkey / C state=1 t=65.','key=Left Softkey / Menu state=1 t=65.'),
                         text+'\nheader cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3b'):
            with self.assertRaises(ValueError):check(mutation,initial,final,'cancel')
        text,initial,final=fixture('erase')
        self.assertEqual(final[3578:3610],b'\xff'*32)
        self.assertEqual(initial[:3578]+initial[3610:],final[:3578]+final[3610:])

    def test_wrong_pin2_preserves_every_record_and_cold_save_requires_retained_attempts(self):
        text,initial,final=fixture('wrong')
        check(text,initial,final,'wrong')
        self.assertEqual([index for index,(a,b) in enumerate(zip(initial,final)) if a!=b],[3517])
        for suffix in ('header cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3b',
                       'header cla=a0 ins=04 p1=00 p2=00 p3=00 selected=6f3a'):
            with self.assertRaises(ValueError):check(text+'\n'+suffix,initial,final,'wrong')
        text,initial,final=fixture('write',2)
        check(text,initial,final,'write',pin2_attempts=2)
        with self.assertRaises(ValueError):check(text,initial,final,'write')

    def test_cold_read_cannot_heal_an_unverified_pin2_allowance(self):
        text,initial,final=fixture('read',2)
        check(text,initial,final,'read',pin2_attempts=2)
        repaired=bytearray(final)
        repaired[3517]=3
        with self.assertRaises(ValueError):check(text,initial,bytes(repaired),'read',pin2_attempts=2)


if __name__=='__main__':unittest.main()
