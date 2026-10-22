import unittest
import io
from unittest.mock import patch
from tools.noki6250_pin_retry_check import registered_card
from tools.run_noki6250_pin2_cold import check_cold_input
from tools import run_noki6250_pin2_cold as runner


class ColdPin2InputTest(unittest.TestCase):
    def test_puk2_recovery_requires_physical_exhaustion_plan(self):
        with patch('sys.argv',['runner','unused','--recover-puk2']), \
                patch('sys.stderr',new_callable=io.StringIO) as output:
            with self.assertRaises(SystemExit) as error:
                runner.main()
        self.assertEqual(error.exception.code,2)
        self.assertIn('--recover-puk2 requires --exhaust-pin2',output.getvalue())

    def test_blocked_and_recovered_inputs_retain_their_actual_credentials(self):
        blocked=bytearray(self.card())
        blocked[3517]=0
        check_cold_input(bytes(blocked),bytes(blocked),retries=0)
        recovered=bytearray(self.card())
        recovered[3492:3500]=b'2468\xff\xff\xff\xff'
        check_cold_input(bytes(recovered),bytes(recovered),pin2='2468')
        with self.assertRaises(ValueError):
            check_cold_input(bytes(recovered),bytes(recovered))

    def test_incompatible_phase_plans_are_rejected_before_launch(self):
        with patch('sys.argv',['runner','unused','--reject-first','--exhaust-pin2']), \
                patch('sys.stderr',new_callable=io.StringIO) as output:
            with self.assertRaises(SystemExit) as error:
                runner.main()
        self.assertEqual(error.exception.code,2)
        self.assertIn('select different phase plans',output.getvalue())

    def card(self):
        card=bytearray(registered_card())
        card[3492:3500]=b'8765\xff\xff\xff\xff'
        return bytes(card)

    def test_entire_installed_card_is_the_cold_input(self):
        check_cold_input(self.card(),self.card())

    def test_failed_allowance_is_retained_not_repaired(self):
        failed=bytearray(self.card())
        failed[3517]=2
        check_cold_input(bytes(failed),bytes(failed),retries=2)
        with self.assertRaises(ValueError):
            check_cold_input(bytes(failed),bytes(failed))
        with self.assertRaises(ValueError):
            check_cold_input(self.card(),bytes(failed),retries=2)

    def test_default_or_repaired_card_is_not_a_cold_installation(self):
        with self.assertRaises(ValueError):
            check_cold_input(registered_card(),self.card())
        with self.assertRaises(ValueError):
            check_cold_input(self.card(),registered_card())

    def test_equal_but_unrelated_mutations_are_not_accepted(self):
        for offset in (0,1604,3484,3492,3516,3517,len(self.card())-1):
            changed=bytearray(self.card())
            changed[offset]^=1
            with self.subTest(offset=offset),self.assertRaises(ValueError):
                check_cold_input(bytes(changed),bytes(changed))


if __name__=='__main__':unittest.main()
