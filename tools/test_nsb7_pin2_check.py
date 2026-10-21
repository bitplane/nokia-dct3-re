"""PIN2 selector/retry isolation and complete cold credential transitions."""
import unittest

from tools.nsb7_pin2_check import check_cards
from tools.nsb7_pin_change_check import check_change
from tools.nsb7_pin_check import serialized_profile
from tools.sim_security_trace_check import CHV_OFFSET, ATTEMPTS_OFFSET, padded_pin
from tools.test_nsb7_pin_change_check import fixture


class Pin2Test(unittest.TestCase):
    def test_physical_pin2_outcomes(self):
        check_change(fixture(False, pin2=True), False, pin2=True)
        check_change(fixture(True, pin2=True), True, pin2=True)
        check_change(fixture(False, pin2=True, old='8765', new='5678'), False,
                     pin2=True, old='8765', new='5678')

    def test_pin1_selector_not_pin2_evidence(self):
        with self.assertRaises(ValueError):
            check_change(fixture(False), False, pin2=True)

    def test_wrong_pin2_must_not_reduce_pin1(self):
        with self.assertRaises(ValueError):
            check_change(fixture(True, pin2=True).replace('chv=3/2', 'chv=2/3'), True, pin2=True)

    def test_exact_card_chain(self):
        initial = serialized_profile()
        wrong = bytearray(initial)
        wrong[ATTEMPTS_OFFSET+1] = 2
        changed = bytearray(initial)
        changed[CHV_OFFSET+8:CHV_OFFSET+16] = padded_pin('8765')
        check_cards([(initial, bytes(wrong)), (initial, bytes(changed)), (bytes(changed), initial)])
        wrong[ATTEMPTS_OFFSET] = 2
        with self.assertRaises(ValueError):
            check_cards([(initial, bytes(wrong)), (initial, bytes(changed)), (bytes(changed), initial)])


if __name__ == '__main__':
    unittest.main()
