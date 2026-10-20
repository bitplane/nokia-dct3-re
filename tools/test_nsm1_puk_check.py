import unittest
from tools.nsm1_puk_check import transition,recovery
from tools.sim_security_trace_check import ATTEMPTS_OFFSET,CHV_OFFSET,padded_pin
from tools.make_sim_card_profile import make_profile


class PukTests(unittest.TestCase):
    def test_exact_transitions(self):
        card=bytearray(make_profile(True));card[ATTEMPTS_OFFSET]=1
        exhausted=transition(card,'exhaust')
        self.assertEqual(exhausted[ATTEMPTS_OFFSET],0)
        wrong=bytearray(exhausted);wrong[ATTEMPTS_OFFSET+2]=9
        self.assertEqual(transition(exhausted,'wrong'),wrong)
        recovered=bytearray(exhausted);recovered[ATTEMPTS_OFFSET]=3
        recovered[CHV_OFFSET:CHV_OFFSET+8]=padded_pin('4321')
        self.assertEqual(transition(exhausted,'recover'),recovered)
        self.assertEqual(transition(recovered,'cold'),recovered)
        with self.assertRaises(ValueError):transition(card,'unknown')

    def test_missing_physical_input_rejected(self):
        for wrong in (False,True):
            with self.assertRaisesRegex(ValueError,'physical'):
                recovery('',wrong)


if __name__=='__main__':unittest.main()
