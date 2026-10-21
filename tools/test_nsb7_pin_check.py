"""Physical SIM VERIFY and complete retry-storage acceptance tests."""
import unittest

from tools.nsb7_pin_check import check_trace, check_cards
from tools.make_sim_card_profile import make_profile
from tools.sim_security_trace_check import ATTEMPTS_OFFSET


def fixture(correct):
    digits = '1234' if correct else '4321'
    lines = []
    for index, key in enumerate(list(digits) + ['Menu']):
        code = 0x19 if key == 'Menu' else int(key)
        lines += [f'nsb7_pin_input: correct={int(correct)} key={key} state=1 t={8+index}.000000000',
                  f'nsb7_key_decode: key={code:02x} observer_writes=0',
                  f'nsb7_pin_input: correct={int(correct)} key={key} state=0 t={8+index}.200000000']
    apdu = bytes.fromhex('a020000108') + digits.encode() + b'\xff'*4
    lines += [f'nsb7_sim_tx: index={i} value={byte:02x}' for i, byte in enumerate(apdu, 1)]
    lines += [f'SIM status ins=20 sw={"9000" if correct else "9804"} chv={3 if correct else 2}/3 puk=10/10 enabled=1',
              'nsb7_pin_input: complete']
    return '\n'.join(lines)


class PinCheckTest(unittest.TestCase):
    def test_both_outcomes(self):
        check_trace(fixture(False), False)
        check_trace(fixture(True), True)

    def test_bad_status_or_input(self):
        for old, new in [('sw=9804', 'sw=9000'), ('chv=2/3', 'chv=3/3'),
                         ('key=19 observer_writes=0', 'key=18 observer_writes=0'),
                         ('value=34', 'value=35'), ('nsb7_pin_input: complete', '')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_trace(fixture(False).replace(old, new), False)

    def test_unexpected_security_mutation(self):
        with self.assertRaises(ValueError):
            check_trace(fixture(True) + '\nSIM status ins=24 sw=9000 chv=3/3 puk=10/10 enabled=1', True)

    def test_complete_card_retry_transition(self):
        initial = make_profile(True)
        correct = initial + bytes.fromhex('00f110') + b'\xff'*43
        wrong = bytearray(correct)
        wrong[ATTEMPTS_OFFSET] = 2
        check_cards(initial, bytes(wrong), bytes(wrong), correct)

    def test_card_extra_mutation_or_lost_cold_retry(self):
        initial = make_profile(True)
        correct = initial + bytes.fromhex('00f110') + b'\xff'*43
        wrong = bytearray(correct)
        wrong[ATTEMPTS_OFFSET] = 2
        for cold, final in [(correct, correct), (bytes(wrong), correct[:-1] + b'\x00')]:
            with self.assertRaises(ValueError):
                check_cards(initial, bytes(wrong), cold, final)


if __name__ == '__main__':
    unittest.main()
