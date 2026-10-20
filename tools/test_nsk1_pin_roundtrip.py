import unittest

from tools.nsk1_pin_roundtrip import check_card,check_contacts
from tools.make_sim_card_profile import CHV1,CHV2,PUK1,PUK2
from tools.sim_security_trace_check import AOC_NVRAM_SIZE,CHV_OFFSET,ATTEMPTS_OFFSET,ENABLED_OFFSET


class Nsk1PinTest(unittest.TestCase):
    def test_credentials_and_retry_checks_fail_closed(self):
        card = bytearray(AOC_NVRAM_SIZE)
        card[CHV_OFFSET:CHV_OFFSET+32] = CHV1+CHV2+PUK1+PUK2
        card[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET+4] = bytes((2,3,10,10))
        card[ENABLED_OFFSET] = 1
        check_card(card,2)
        for offset in (CHV_OFFSET,ATTEMPTS_OFFSET,ATTEMPTS_OFFSET+2,ENABLED_OFFSET):
            changed = bytearray(card)
            changed[offset] ^= 1
            with self.assertRaises(ValueError):
                check_card(changed,2)
        with self.assertRaises(ValueError):
            check_card(card[:-1],2)

    def test_contacts_require_all_ordered_edges(self):
        trace = '\n'.join(f'nsk1_pin_contact: phase=wrong index={index} down={down} t=12.0 physical_only=1'
                          for index in range(1,6) for down in (1,0))
        check_contacts(trace,'wrong',5)
        for invalid in (trace.replace('index=3','index=4'),trace+'\nnsk1_pin_contact: phase=wrong malformed'):
            with self.assertRaises(ValueError):
                check_contacts(invalid,'wrong',5)


if __name__ == '__main__':
    unittest.main()
