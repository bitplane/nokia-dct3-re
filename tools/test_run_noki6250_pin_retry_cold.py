import unittest
from tools.run_noki6250_pin_retry_cold import check_phase
from tools.noki6250_pin_retry_check import registered_card
from tools.make_sim_card_profile import make_profile


def fixture(recovery=False):
    initial=make_profile(True)
    failed=bytearray(initial+bytes.fromhex('00f110')+b'\xff'*43)
    failed[3516]=2
    items=([(8+i,'Keypad '+d) for i,d in enumerate('1234')] if recovery else
           [(second,'Keypad 9') for second in range(8,12)])
    items.append((12,'Left Softkey / Menu'))
    if recovery:items.append((34,'Left Softkey / Menu'))
    lines=[]
    for second,key in items:
        lines += [f'6250_pin_retry: key={key} state=1 t={second}.000000000',
                  f'6250_pin_retry: key={key} state=0 t={second}.150000000']
        if second==12:
            lines.append('header cla=a0 ins=20 p1=00 p2=01 p3=08')
            lines.append('SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1' if recovery else
                         'SIM status ins=20 sw=9804 chv=2/3 puk=10/10 enabled=1')
            if recovery:lines.append('LAPDm Location Updating Accept acknowledged')
    lines.append('6250_pin_retry: '+('complete' if recovery else 'wrong_only_complete')+'=1 firmware_state_forcing=0')
    return '\n'.join(lines),bytes(failed) if recovery else initial,registered_card() if recovery else bytes(failed)


class ColdPinRetryTest(unittest.TestCase):
    def test_independent_wrong_and_recovery_contracts(self):
        for recovery in (False,True):
            check_phase(*fixture(recovery),recovery=recovery)

    def test_selectors_and_retry_counts_are_required(self):
        for recovery in (False,True):
            text,initial,final=fixture(recovery)
            for old,new in [('p2=01','p2=02'),('chv=2/3','chv=1/3') if not recovery else ('chv=3/3','chv=2/3'),
                            ('state=0 t=8.','state=1 t=8.'),('complete=1','complete=0')]:
                with self.subTest(recovery=recovery,old=old),self.assertRaises(ValueError):
                    check_phase(text.replace(old,new),initial,final,recovery=recovery)

    def test_cold_card_must_retain_failed_allowance(self):
        text,initial,final=fixture(True)
        changed=bytearray(initial)
        changed[3516]=3
        with self.assertRaises(ValueError):
            check_phase(text,bytes(changed),final,recovery=True)

    def test_unrelated_card_bytes_are_not_repaired(self):
        for recovery in (False,True):
            text,initial,final=fixture(recovery)
            for offset in (0,1604,3484,3517,len(final)-1):
                changed=bytearray(final)
                changed[offset]^=1
                with self.subTest(recovery=recovery,offset=offset),self.assertRaises(ValueError):
                    check_phase(text,initial,bytes(changed),recovery=recovery)

    def test_failed_pin_cannot_register(self):
        text,initial,final=fixture()
        with self.assertRaises(ValueError):
            check_phase(text+'\nLAPDm Location Updating Accept acknowledged',initial,final)


if __name__=='__main__':unittest.main()
