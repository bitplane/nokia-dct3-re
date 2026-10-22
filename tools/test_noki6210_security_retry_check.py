import unittest

from tools.make_sim_card_profile import make_profile
from tools.noki6210_security_retry_check import KEYS,check,check_wrong,check_cold


def fixture():
    lines=[]
    for second,key in KEYS:
        lines.extend([f'6210_pin_retry_input: key={key} state=1 t={second}.000000000',
                      f'6210_pin_retry_input: key={key} state=0 t={second}.150000000'])
        if second==12:
            lines.append('SIM status ins=20 sw=9804 chv=2/3 puk=10/10 enabled=1')
        if second==20:
            lines.extend(['SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1',
                          'LAPDm Location Updating Accept acknowledged nr=1'])
    lines.append('6210_pin_retry_input: complete=1 firmware_state_forcing=0')
    card=bytearray(make_profile(True)+bytes.fromhex('00f110')+b'\xff'*43)
    card[1604:1609]=bytes.fromhex('00f1100001')
    card[1610]=0
    return '\n'.join(lines),bytes(card)


class PinRetryTests(unittest.TestCase):
    def test_wrong_then_correct_pin(self):
        check(*fixture())

    def test_retry_credentials_and_unrelated_bytes_are_protected(self):
        text,original=fixture()
        for index in (0,1604,1610,1700,3484,3500,3516,3520,3569):
            card=bytearray(original)
            card[index]^=1
            with self.subTest(index=index),self.assertRaises(ValueError):
                check(text,bytes(card))

    def test_wrong_status_input_order_and_release_fail(self):
        text,card=fixture()
        for old,new in [('sw=9804','sw=9000'),('chv=2/3','chv=1/3'),
                        ('key=Keypad 9','key=Keypad 8'),('state=0 t=8.150000000','state=0 t=8.250000000'),
                        ('t=20.000000000','t=21.000000000'),
                        ('LAPDm Location Updating Accept acknowledged','missing'),
                        ('complete=1','complete=0')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                check(text.replace(old,new),card)

    def test_duplicate_verify_and_truncated_card_fail(self):
        text,card=fixture()
        with self.assertRaises(ValueError):
            check(text+'\nSIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1',card)
        with self.assertRaises(ValueError):
            check(text,card[:-1])

    def test_failed_retry_is_exact_cold_input(self):
        lines=[]
        for second,key in KEYS[:5]:
            lines.extend([f'6210_pin_retry_input: key={key} state=1 t={second}.000000000',
                          f'6210_pin_retry_input: key={key} state=0 t={second}.150000000'])
        lines.extend(['SIM status ins=20 sw=9804 chv=2/3 puk=10/10 enabled=1',
                      '6210_pin_retry_input: wrong_complete=1 firmware_state_forcing=0'])
        wrong='\n'.join(lines)
        initial=bytearray(make_profile(True)+bytes.fromhex('00f110')+b'\xff'*43)
        initial[3516]=2
        check_wrong(wrong,bytes(initial))
        cold=('6210_security_physical: action=confirm\n'
              'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1\n'
              'LAPDm Location Updating Accept acknowledged nr=1\n'
              '6210_security_physical: action=menu\n')
        final=fixture()[1]
        check_cold(cold,bytes(initial),final)
        for index in (0,3484,3516,3520,3569):
            mutated=bytearray(initial)
            mutated[index]^=1
            with self.subTest(index=index),self.assertRaises(ValueError):
                check_cold(cold,bytes(mutated),final)
        with self.assertRaises(ValueError):
            check_wrong(wrong,final)
        with self.assertRaises(ValueError):
            check_cold(cold.replace('sw=9000','sw=9804'),bytes(initial),final)


if __name__=='__main__':
    unittest.main()
