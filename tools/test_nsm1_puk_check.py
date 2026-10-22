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

    def test_all_retry_transitions_and_recovery_after_rejection(self):
        card=bytearray(make_profile(True))
        card[ATTEMPTS_OFFSET]=0
        for count in range(1,11):
            card[ATTEMPTS_OFFSET+2]=count
            expected=bytearray(card);expected[ATTEMPTS_OFFSET+2]=count-1
            self.assertEqual(transition(card,'wrong'),expected)
            restored=bytearray(card)
            restored[ATTEMPTS_OFFSET]=3
            restored[ATTEMPTS_OFFSET+2]=10
            restored[CHV_OFFSET:CHV_OFFSET+8]=padded_pin('4321')
            self.assertEqual(transition(card,'recover'),restored)
        card[ATTEMPTS_OFFSET+2]=0
        for mode in ('wrong','recover'):
            with self.assertRaises(ValueError):transition(card,mode)
        for mode in ('exhaust','wrong','recover','cold'):
            with self.assertRaises(ValueError):transition(b'',mode)

    def test_terminal_puk_status_with_physical_command(self):
        keys=(1,2,3,4,5,6,7,9,0x19)+(4,3,2,1,0x19)*2
        command=bytes.fromhex('a02c000010')+b'12345679'+padded_pin('4321')
        trace='\n'.join([*[f'nsm1_puk_contact: key={key:02x} down={edge} '
                            for key in keys for edge in (1,0)],
                         *[f'nsm1_sim_tx: index={index} value={word:02x}' for index,word in enumerate(command,1)],
                         'SIM status ins=2c sw=9840 chv=0/3 puk=0/10 enabled=1'])
        recovery(trace,True,attempts_before=1)
        for bad in (trace.replace('sw=9840','sw=9804'),trace.replace('key=19','key=17'),
                    trace.replace('value=2c','value=20')):
            with self.assertRaises(ValueError):recovery(bad,True,attempts_before=1)
        with self.assertRaises(ValueError):recovery(trace,True,attempts_before=0)


if __name__=='__main__':unittest.main()
