import unittest
from unittest.mock import patch

from tools.make_sim_card_profile import make_profile
from tools.run_noki8850_pin2_change import registered_card,check_phase,check_puk2_phase


TRACE='\n'.join((
    'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1',
    '8850_pin2: key=Menu state=1 t=58.000000000',
    'sim_device: header cla=a0 ins=24 p1=00 p2=02 p3=10',
    'SIM status ins=24 sw=9000 chv=3/3 puk=10/10 enabled=1',
    '8850_pin2: key=Menu state=1 t=63.000000000',
    '8850_pin2: complete=1 firmware_state_forcing=0',
))


class Pin2ChangeTest(unittest.TestCase):
    def test_puk2_exhaustion_has_exact_last_allowance_status(self):
        for allowance in range(10,0,-1):
            blocked=bytearray(registered_card('5678'))
            blocked[3517]=0
            blocked[3519]=allowance
            rejected=bytearray(blocked)
            rejected[3519]-=1
            status='9840' if allowance==1 else '9804'
            trace=('sim_device: header cla=a0 ins=20 p1=00 p2=01 p3=08\n'
                   f'SIM status ins=20 sw=9000 chv=3/0 puk=10/{allowance} enabled=1\n'
                   '8850_pin2: key=Menu state=1 t=62.000000000\n'
                   'sim_device: header cla=a0 ins=2c p1=00 p2=02 p3=10\n'
                   f'SIM status ins=2c sw={status} chv=3/0 puk=10/{allowance-1} enabled=1\n'
                   '8850_pin2: key=Menu state=1 t=68.000000000\n'
                   '8850_pin2: complete=1 firmware_state_forcing=0')
            with self.subTest(allowance=allowance):
                check_puk2_phase(trace,bytes(blocked),bytes(rejected),rejected=True,puk2_attempts=allowance)
                with self.assertRaises(ValueError):
                    check_puk2_phase(trace,bytes(blocked),bytes(blocked),rejected=True,puk2_attempts=allowance)

    def test_permanent_puk2_retains_pin1_but_issues_no_second_credential_apdu(self):
        blocked=bytearray(registered_card('5678'))
        blocked[3517]=blocked[3519]=0
        trace=('sim_device: header cla=a0 ins=20 p1=00 p2=01 p3=08\n'
               'SIM status ins=20 sw=9000 chv=3/0 puk=10/0 enabled=1\n'
               '8850_pin2: key=Menu state=1 t=43.000000000\n'
               '8850_pin2: key=Keypad 9 state=1 t=46.000000000\n'
               '8850_pin2: key=End state=1 t=48.000000000\n'
               '8850_pin2: key=Menu state=1 t=49.000000000\n'
               '8850_pin2: complete=1 firmware_state_forcing=0')
        check_puk2_phase(trace,bytes(blocked),bytes(blocked),permanent=True,puk2_attempts=0)
        for extra in ('SIM status ins=2c sw=9840 chv=3/0 puk=10/0 enabled=1',
                      'sim_device: header cla=a0 ins=20 p1=00 p2=02 p3=08',
                      'GSM service establish sapi=0 pd=03 message=05 '):
            with self.assertRaises(ValueError):
                check_puk2_phase(trace+'\n'+extra,bytes(blocked),bytes(blocked),permanent=True,puk2_attempts=0)
        with self.assertRaises(ValueError):
            check_puk2_phase(trace,bytes(blocked),registered_card('2468'),permanent=True,puk2_attempts=0)

    def test_wrong_puk2_preserves_pin2_and_forbids_post_failure_verify(self):
        blocked=bytearray(registered_card('5678'))
        blocked[3517]=0
        rejected=bytearray(blocked)
        rejected[3519]=9
        trace=('sim_device: header cla=a0 ins=20 p1=00 p2=01 p3=08\n'
               'SIM status ins=20 sw=9000 chv=3/0 puk=10/10 enabled=1\n'
               '8850_pin2: key=Menu state=1 t=62.000000000\n'
               'sim_device: header cla=a0 ins=2c p1=00 p2=02 p3=10\n'
               'SIM status ins=2c sw=9804 chv=3/0 puk=10/9 enabled=1\n'
               '8850_pin2: key=Menu state=1 t=68.000000000\n'
               '8850_pin2: complete=1 firmware_state_forcing=0')
        check_puk2_phase(trace,bytes(blocked),bytes(rejected),rejected=True)
        for bad in (trace.replace('sw=9804','sw=9000'),
                    trace+'\nsim_device: header cla=a0 ins=20 p1=00 p2=02 p3=08\n'
                    'SIM status ins=20 sw=9840 chv=3/0 puk=10/9 enabled=1'):
            with self.assertRaises(ValueError):
                check_puk2_phase(bad,bytes(blocked),bytes(rejected),rejected=True)
        for offset in (0,3484,3492,3500,3516,3517,3518,3519,3520):
            bad=bytearray(rejected)
            bad[offset]^=1
            with self.subTest(offset=offset),self.assertRaises(ValueError):
                check_puk2_phase(trace,bytes(blocked),bytes(bad),rejected=True)

    def test_each_pin2_rejection_decrements_only_its_allowance(self):
        for allowance in (3,2,1):
            initial=bytearray(make_profile(True) if allowance==3 else registered_card('5678'))
            initial[3517]=allowance
            final=bytearray(registered_card('5678'))
            final[3517]=allowance-1
            status='9840' if allowance==1 else '9804'
            trace=TRACE.replace('ins=24 sw=9000 chv=3/3',f'ins=24 sw={status} chv=3/{allowance-1}')
            with self.subTest(allowance=allowance):
                check_phase(trace,bytes(initial),bytes(final),rejected=True,pin2_attempts=allowance)
                with self.assertRaises(ValueError):
                    check_phase(trace,bytes(initial),registered_card('2468'),rejected=True,pin2_attempts=allowance)

    def test_puk2_recovers_only_the_blocked_second_credential(self):
        blocked=bytearray(registered_card('5678'))
        blocked[3517]=0
        post_verify=('sim_device: header cla=a0 ins=20 p1=00 p2=02 p3=08\n'
                     'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1\n')
        trace=('sim_device: header cla=a0 ins=20 p1=00 p2=01 p3=08\n'
               'SIM status ins=20 sw=9000 chv=3/0 puk=10/10 enabled=1\n'
               '8850_pin2: key=Menu state=1 t=62.000000000\n'
               'sim_device: header cla=a0 ins=2c p1=00 p2=02 p3=10\n'
               'SIM status ins=2c sw=9000 chv=3/3 puk=10/10 enabled=1\n'+post_verify+
               '8850_pin2: key=Menu state=1 t=68.000000000\n'
               '8850_pin2: complete=1 firmware_state_forcing=0')
        check_puk2_phase(trace,bytes(blocked),registered_card('2468'))
        for bad in (trace.replace('p2=02','p2=00'),trace.replace(post_verify,''),
                    trace+'\nSIM status ins=24 sw=9000 chv=3/3 puk=10/10 enabled=1'):
            with self.assertRaises(ValueError):check_puk2_phase(bad,bytes(blocked),registered_card('2468'))
        for offset in (0,3484,3500,3516,3518,3519,3520):
            final=bytearray(registered_card('2468'))
            final[offset]^=1
            with self.subTest(offset=offset),self.assertRaises(ValueError):
                check_puk2_phase(trace,bytes(blocked),bytes(final))

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
