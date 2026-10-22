import unittest
from tools.noki6210_fdn_record_check import check,expected_card,input_contacts


def fixture(mode):
    lines=[]
    for second,key in input_contacts(mode):
        lines += [f'6210_fdn_input: key={key} state=1 t={second}.000000000',
                  f'6210_fdn_input: key={key} state=0 t={second}.150000000']
        if second==12:
            lines += ['header cla=a0 ins=20 p1=00 p2=01 p3=08',
                      'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1']
            for fid in ('6f7e','6f07'):
                lines += [f'header cla=a0 ins=44 p1=00 p2=00 p3=00 selected={fid}',
                          'SIM status ins=44 sw=9000 chv=3/3 puk=10/10 enabled=1']
            lines += ['LAPDm Location Updating Accept acknowledged nr=1']
            lines += [f'header cla=a0 ins=b2 p1={i:02x} p2=04 p3=20 selected=6f3b' for i in range(1,11)]
        if ((second==57 and mode in ('write','wrong')) or (second==61 and mode=='edit') or
                (second==62 and mode in ('cancel','erase'))):
            lines += ['header cla=a0 ins=20 p1=00 p2=02 p3=08',
                      ('SIM status ins=20 sw=9804 chv=3/2 puk=10/10 enabled=1' if mode=='wrong' else
                       'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1')]
        if ((second==71 and mode=='write') or (second==73 and mode=='edit') or
                (second==65 and mode=='erase')):
            lines += ['header cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3b',
                      'SIM status ins=dc sw=9000 chv=3/3 puk=10/10 enabled=1']
            if mode in ('write','edit'):
                lines.append('header cla=a0 ins=b2 p1=01 p2=04 p3=20 selected=6f3b')
    lines.append('6210_fdn_input: complete=1 firmware_state_forcing=0')
    final=bytearray(expected_card(mode not in ('erase','empty','wrong'),mode in ('edit','edited-read')))
    if mode=='wrong':
        final[3517]=2
    return ('\n'.join(lines),expected_card(mode not in ('write','empty','wrong'),mode=='edited-read'),bytes(final))


class FdnRecordTests(unittest.TestCase):
    def test_protected_write_and_cold_read(self):
        for mode in ('write','read','edit','edited-read','cancel','erase','empty','wrong'):
            with self.subTest(mode=mode):
                check(*fixture(mode),mode)

    def test_wrong_record_target_pin2_read_census_or_input_fails(self):
        text,initial,final=fixture('write')
        for old,new in (('ins=dc p1=01','ins=dc p1=02'),
                        ('ins=dc sw=9000','ins=dc sw=9804'),
                        ('ins=20 p1=00 p2=02','ins=20 p1=00 p2=01'),
                        ('p1=0a p2=04','p1=09 p2=04'),
                        ('t=71.000000000','t=72.000000000'),
                        ('key=Keypad 5','key=Keypad 6')):
            with self.subTest(old=old),self.assertRaises(ValueError):
                check(text.replace(old,new),initial,final,'write')

    def test_whole_source_and_result_are_checked(self):
        for mode in ('write','read','edit','edited-read','cancel','erase','empty','wrong'):
            text,initial,final=fixture(mode)
            for index in (0,1604,3484,3517,3575,3578,3596,3600,3610):
                changed=bytearray(final)
                changed[index]^=1
                with self.subTest(mode=mode,index=index),self.assertRaises(ValueError):
                    check(text,initial,bytes(changed),mode)
            with self.assertRaises(ValueError):
                check(text,initial[:-1],final,mode)

    def test_read_does_not_write_or_authenticate_pin2(self):
        text,initial,final=fixture('read')
        for extra in ('header cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3b',
                      'header cla=a0 ins=20 p1=00 p2=02 p3=08'):
            with self.subTest(extra=extra),self.assertRaises(ValueError):
                check(text+'\n'+extra,initial,final,'read')

    def test_edit_requires_protected_number_before_the_update(self):
        text,initial,final=fixture('edit')
        for old,new in (('key=Right Softkey / C','key=Scroll Down'),
                        ('key=Keypad 6 state=1 t=70.','key=Keypad 5 state=1 t=70.'),
                        ('t=73.000000000','t=74.000000000'),
                        ('ins=20 p1=00 p2=02','ins=20 p1=00 p2=01')):
            with self.subTest(old=old),self.assertRaises(ValueError):
                check(text.replace(old,new),initial,final,'edit')
        with self.assertRaises(ValueError):
            check(text,initial,expected_card(True),'edit')

    def test_erase_and_cancel_have_distinct_confirmations_and_mutations(self):
        for mode in ('cancel','erase'):
            text,initial,final=fixture(mode)
            wrong='Left Softkey / Menu' if mode=='cancel' else 'Right Softkey / C'
            right='Right Softkey / C' if mode=='cancel' else 'Left Softkey / Menu'
            with self.subTest(mode=mode),self.assertRaises(ValueError):
                check(text.replace(f'key={right} state=1 t=65.',f'key={wrong} state=1 t=65.'),initial,final,mode)
            other=expected_card(mode=='erase')
            with self.assertRaises(ValueError):
                check(text,initial,other,mode)
        text,initial,final=fixture('cancel')
        with self.assertRaises(ValueError):
            check(text+'\nheader cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3b',initial,final,'cancel')

    def test_rejected_add_has_no_update_and_cold_retry_restores_allowance(self):
        text,initial,final=fixture('wrong')
        with self.assertRaises(ValueError):
            check(text+'\nheader cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3b',initial,final,'wrong')
        text,initial,final=fixture('write')
        initial=bytearray(initial)
        initial[3517]=2
        text=text.replace('ins=20 sw=9000 chv=3/3','ins=20 sw=9000 chv=3/2',1)
        check(text,bytes(initial),final,'write',pin2_attempts=2)
        with self.assertRaises(ValueError):
            check(text,bytes(initial),final,'write')


if __name__=='__main__':
    unittest.main()
