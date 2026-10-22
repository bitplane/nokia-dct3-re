import unittest
from tools.noki6210_fdn_check import check,input_contacts
from tools.noki6210_puk_check import profile
from tools.sim_fdn_profile import extend
from tools.nsk3_phonebook_check import expected_record


def validity(instruction,fid):
    return [f'header cla=a0 ins={instruction:02x} p1=00 p2=00 p3=00 selected={fid:04x}',
            f'SIM status ins={instruction:02x} sw=9000 chv=3/3 puk=10/10 enabled=1']


def fixture(phase):
    lines=[]
    for second,key in input_contacts(phase):
        lines += [f'6210_fdn_input: key={key} state=1 t={second}.000000000',
                  f'6210_fdn_input: key={key} state=0 t={second}.150000000']
        if second==12:
            lines += ['header cla=a0 ins=20 p1=00 p2=01 p3=08',
                      'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1']
            if phase in (1,2):
                lines += validity(0x44,0x6f7e)+validity(0x44,0x6f07)
            lines += ['LAPDm Location Updating Accept acknowledged nr=1']
            lines += [f'header cla=a0 ins=b2 p1={i:02x} p2=04 p3=20 selected=6f3b' for i in range(1,11)]
        if second==51:
            lines += ['header cla=a0 ins=20 p1=00 p2=02 p3=08',
                      'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1']
            lines += validity(4 if phase==0 else 0x44,0x6f3a)
            if phase==0:
                lines += validity(0x44,0x6f7e)+validity(0x44,0x6f07)
    lines += ['6210_fdn_input: complete=1 firmware_state_forcing=0']
    card=bytearray(extend(profile(3,True)))
    card[3575]=0 if phase<2 else 1
    return '\n'.join(lines),bytes(card)


class FdnLifecycleTests(unittest.TestCase):
    def test_disable_preserves_the_existing_fdn_record(self):
        text,card=fixture(2)
        populated=bytearray(card)
        populated[3578:3610]=expected_record()
        check(text,bytes(populated),2,record=expected_record())
        with self.assertRaises(ValueError):
            check(text,bytes(populated),2)
        with self.assertRaises(ValueError):
            check(text,bytes(populated),2,record=b'A')

    def test_wrong_pin2_activation_only_consumes_its_retry(self):
        text,_=fixture(0)
        text='\n'.join(line for line in text.splitlines() if 'ins=04' not in line and 'ins=44' not in line)
        for state in (0,1):
            text=text.replace(f'key=Keypad 8 state={state} t=50.',f'key=Keypad 5 state={state} t=50.')
        before,last,after=text.rpartition('SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1')
        self.assertTrue(last)
        text=before+'SIM status ins=20 sw=9804 chv=3/2 puk=10/10 enabled=1'+after
        card=bytearray(extend(profile(3,True)))
        card[3517]=2
        check(text,bytes(card),0,rejected=True)
        for index in (3492,3516,3517,3575,3578):
            changed=bytearray(card)
            changed[index]^=1
            with self.subTest(index=index),self.assertRaises(ValueError):
                check(text,bytes(changed),0,rejected=True)
        with self.assertRaises(ValueError):
            check(text+'\n'+'\n'.join(validity(4,0x6f3a)),bytes(card),0,rejected=True)

    def test_cold_activation_restores_pin2_retry_only_on_success(self):
        text,card=fixture(0)
        text=text.replace('ins=20 sw=9000 chv=3/3','ins=20 sw=9000 chv=3/2',1)
        check(text,card,0,initial_pin2_attempts=2)
        with self.assertRaises(ValueError):
            check(text,card,0)

    def test_all_four_physical_phases(self):
        for phase in range(4):
            with self.subTest(phase=phase):
                check(*fixture(phase),phase)

    def test_wrong_validity_chv_records_input_or_order_is_rejected(self):
        text,card=fixture(0)
        for old,new in (('ins=04 p1=00','ins=44 p1=00'),
                        ('ins=20 p1=00 p2=02','ins=20 p1=00 p2=01'),
                        ('p1=0a p2=04','p1=09 p2=04'),
                        ('key=Scroll Up','key=Scroll Down'),
                        ('t=51.000000000','t=52.000000000'),
                        ('complete=1','complete=0')):
            with self.subTest(old=old),self.assertRaises(ValueError):
                check(text.replace(old,new),card,0)

    def test_no_other_card_mutation_is_allowed(self):
        for phase in range(4):
            text,card=fixture(phase)
            for index in (0,1604,3484,3492,3517,3574,3575,3578):
                changed=bytearray(card)
                changed[index]^=1
                with self.subTest(phase=phase,index=index),self.assertRaises(ValueError):
                    check(text,bytes(changed),phase)

    def test_cold_observation_must_not_verify_pin2_or_toggle_fdn(self):
        text,card=fixture(1)
        with self.assertRaises(ValueError):
            check(text+'\nSIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1',card,1)
        with self.assertRaises(ValueError):
            check(text+'\n'+'\n'.join(validity(4,0x6f3a)),card,1)

    def test_unknown_phase_is_rejected(self):
        with self.assertRaises(ValueError):
            input_contacts(4)
