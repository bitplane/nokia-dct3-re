from pathlib import Path
import unittest
from unittest.mock import patch

from tools.nsb1_puk_roundtrip import (
    NEW_PIN, blocked_profile, check_card, check_cold, check_unblock,
    check_failed_card, check_wrong,
)
from tools.sim_security_trace_check import (
    AOC_NVRAM_SIZE, ATTEMPTS_OFFSET, CHV_OFFSET, ENABLED_OFFSET,
)


class PukRoundtripTest(unittest.TestCase):
    def card(self):
        card = bytearray(blocked_profile())
        card.extend(bytes(AOC_NVRAM_SIZE-len(card)))
        card[CHV_OFFSET:CHV_OFFSET+8] = NEW_PIN
        card[ATTEMPTS_OFFSET] = 3
        return bytes(card)

    def contacts(self,phase,count):
        return '\n'.join(f'nsb1_pin_contact: phase={phase} index={index} down={down} t=12.0 physical_only=1'
                         for index in range(1,count+1) for down in (1,0))

    def unblock(self):
        return '\n'.join([self.contacts('puk',9),self.contacts('new',5),
            self.contacts('confirm',5),'SIM status ins=2c sw=9000 chv=3/3 puk=10/10 enabled=1'])

    def cold(self):
        return '\n'.join([self.contacts('pin',5),self.contacts('phone',6),
            'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1'])

    def test_blocked_fixture_changes_only_retry(self):
        from tools.make_sim_card_profile import make_profile
        original = make_profile(True)
        card = blocked_profile()
        self.assertEqual([i for i,(a,b) in enumerate(zip(original,card)) if a != b],[ATTEMPTS_OFFSET])
        self.assertEqual(len(original),len(card))
        self.assertEqual(card[ATTEMPTS_OFFSET],0)

    def test_unblock_requires_exact_physical_transaction(self):
        with patch('tools.nsb1_puk_roundtrip.check_frame') as frame:
            check_unblock(self.unblock(),self.card(),Path('.'))
            self.assertEqual(frame.call_count,3)
            for trace in (self.unblock().replace('phase=confirm','phase=other'),
                          self.unblock().replace('sw=9000','sw=9804'),
                          self.unblock()+'\nSIM status ins=2c sw=9000 chv=3/3 puk=10/10 enabled=1',
                          self.unblock().replace('ins=2c','ins=20')):
                with self.subTest(trace=trace),self.assertRaises(ValueError):
                    check_unblock(trace,self.card(),Path('.'))

    def test_cold_requires_new_verification_not_another_unblock(self):
        with patch('tools.nsb1_puk_roundtrip.check_frame') as frame:
            check_cold(self.cold(),self.card(),Path('.'))
            self.assertEqual(frame.call_count,4)
            for trace in (self.cold().replace('ins=20','ins=2c'),
                          self.cold().replace('phase=phone','phase=other')):
                with self.subTest(trace=trace),self.assertRaises(ValueError):
                    check_cold(trace,self.card(),Path('.'))

    def test_storage_rejects_collateral_security_changes(self):
        check_card(self.card())
        for offset in (CHV_OFFSET,CHV_OFFSET+8,CHV_OFFSET+16,CHV_OFFSET+24,
                       ATTEMPTS_OFFSET,ATTEMPTS_OFFSET+2,ENABLED_OFFSET):
            card = bytearray(self.card())
            card[offset] ^= 1
            with self.subTest(offset=offset),self.assertRaises(ValueError):
                check_card(card)
        with self.assertRaises(ValueError):
            check_card(self.card()[:-1])

    def test_wrong_puk_keeps_credentials_and_decrements_only_puk1(self):
        card = bytearray(blocked_profile())
        card.extend(bytes(AOC_NVRAM_SIZE-len(card)))
        card[ATTEMPTS_OFFSET+2] = 9
        trace = self.unblock().replace('sw=9000 chv=3/3 puk=10/10',
                                      'sw=9804 chv=0/3 puk=9/10')
        with patch('tools.nsb1_puk_roundtrip.check_frame') as frame:
            check_wrong(trace,bytes(card),Path('.'))
            self.assertEqual(frame.call_count,2)
            for bad_trace in (self.unblock(),trace.replace('phase=puk','phase=other')):
                with self.subTest(trace=bad_trace),self.assertRaises(ValueError):
                    check_wrong(bad_trace,bytes(card),Path('.'))
        for offset in (CHV_OFFSET,CHV_OFFSET+16,ATTEMPTS_OFFSET,ATTEMPTS_OFFSET+2):
            broken = bytearray(card)
            broken[offset] ^= 1
            with self.subTest(offset=offset),self.assertRaises(ValueError):
                check_failed_card(bytes(broken))


if __name__ == '__main__':
    unittest.main()
