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
        if second==57 and mode=='write':
            lines += ['header cla=a0 ins=20 p1=00 p2=02 p3=08',
                      'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1']
        if second==71:
            lines += ['header cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3b',
                      'SIM status ins=dc sw=9000 chv=3/3 puk=10/10 enabled=1',
                      'header cla=a0 ins=b2 p1=01 p2=04 p3=20 selected=6f3b']
    lines.append('6210_fdn_input: complete=1 firmware_state_forcing=0')
    return '\n'.join(lines),expected_card(mode=='read'),expected_card(True)


class FdnRecordTests(unittest.TestCase):
    def test_protected_write_and_cold_read(self):
        for mode in ('write','read'):
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
        for mode in ('write','read'):
            text,initial,final=fixture(mode)
            for index in (0,1604,3484,3517,3575,3578,3596,3610):
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


if __name__=='__main__':
    unittest.main()
