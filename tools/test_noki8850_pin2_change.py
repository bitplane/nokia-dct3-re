import unittest
from unittest.mock import patch

from tools.make_sim_card_profile import make_profile
from tools.run_noki8850_pin2_change import registered_card,check_phase


TRACE='\n'.join((
    'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1',
    '8850_pin2: key=Menu state=1 t=58.000000000',
    'sim_device: header cla=a0 ins=24 p1=00 p2=02 p3=10',
    'SIM status ins=24 sw=9000 chv=3/3 puk=10/10 enabled=1',
    '8850_pin2: key=Menu state=1 t=63.000000000',
    '8850_pin2: complete=1 firmware_state_forcing=0',
))


class Pin2ChangeTest(unittest.TestCase):
    def test_rejected_change_preserves_pin2_and_cold_recovery_restores_retry(self):
        rejected=bytearray(registered_card('5678'))
        rejected[3517]=2
        trace=TRACE.replace('ins=24 sw=9000 chv=3/3','ins=24 sw=9804 chv=3/2')
        check_phase(trace,make_profile(True),bytes(rejected),rejected=True)
        check_phase(TRACE,bytes(rejected),registered_card('2468'),prior_rejection=True)
        for bad in (registered_card('2468'),registered_card('5678')):
            with self.assertRaises(ValueError):
                check_phase(trace,make_profile(True),bad,rejected=True)
        with self.assertRaises(ValueError):
            check_phase(TRACE,registered_card('5678'),registered_card('2468'),prior_rejection=True)

    def setUp(self):
        self.contexts=[patch('tools.run_noki8850_pin2_change.'+name)
                       for name in ('contacts','check_pin_inputs','registration')]
        for context in self.contexts:context.start()
        self.addCleanup(lambda:[context.stop() for context in reversed(self.contexts)])

    def test_change_and_cold_new_credential_use(self):
        check_phase(TRACE,make_profile(True),registered_card('2468'))
        check_phase(TRACE,registered_card('2468'),registered_card('5678'),True)

    def test_cold_phase_cannot_reseed_the_original_card(self):
        with self.assertRaises(ValueError):
            check_phase(TRACE,make_profile(True),registered_card('5678'),True)
        with self.assertRaises(ValueError):
            check_phase(TRACE,registered_card('5678'),registered_card('5678'),True)

    def test_unrelated_card_changes_are_rejected(self):
        for offset in (0,1604,3484,3500,3516,3517,3518,3520):
            card=bytearray(registered_card('2468'))
            card[offset]^=1
            with self.subTest(offset=offset),self.assertRaises(ValueError):
                check_phase(TRACE,make_profile(True),bytes(card))

    def test_wrong_selector_extra_change_and_early_result_fail(self):
        for trace in (TRACE.replace('p2=02','p2=01'),
                      TRACE+'\nSIM status ins=24 sw=9000 chv=3/3 puk=10/10 enabled=1',
                      TRACE.replace('state=1 t=58.','state=1 t=59.'),
                      TRACE+'\nSIM status ins=2c sw=9000 chv=3/3 puk=10/10 enabled=1'):
            with self.subTest(trace=trace[-80:]),self.assertRaises(ValueError):
                check_phase(trace,make_profile(True),registered_card('2468'))


if __name__=='__main__':unittest.main()
