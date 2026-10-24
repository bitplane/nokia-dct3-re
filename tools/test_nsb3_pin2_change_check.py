import unittest

from tools.nsb3_pin2_change_check import check_trace, check_pin2_card
from tools.nsb3_pin_change_check import CHANGE_KEYS
from tools.test_nsb3_pin_request_check import trace
from tools.sim_security_trace_check import CHV_OFFSET, padded_pin
from tools.make_sim_card_profile import make_current_profile


def pin2_trace(restore=False, wrong=False):
    old, new = ('8765', '5678') if restore else ('5678', '8765')
    if wrong:
        old = '5679'
    lines = trace(3).splitlines()
    keys = CHANGE_KEYS[:12] + ('Scroll Down', 'Left Softkey / Menu')
    for digits in (old, new, new):
        keys += tuple('Keypad ' + digit for digit in digits) + ('Left Softkey / Menu',)
    lines += [f'nsb3_security_key: key={key} down={state} t=79.0 physical_only=1'
              for key in keys for state in (1, 0)]
    command = bytes.fromhex('a024000210') + padded_pin(old) + padded_pin(new)
    lines += [f'nsb3_sim_wire: direction=TX data={byte:02x} t=88.0 observer_writes=0' for byte in command]
    lines.append(f'SIM status ins=24 sw={"9804" if wrong else "9000"} chv=3/3 puk=10/10 enabled=1')
    return '\n'.join(lines)


class Nsb3Pin2ChangeTest(unittest.TestCase):
    def test_wrong_pin2_and_retry_state_are_not_success(self):
        check_trace(pin2_trace(wrong=True), wrong=True)
        with self.assertRaises(ValueError):
            check_trace(pin2_trace(wrong=True))
        card = bytearray(make_current_profile(True))
        from tools.sim_security_trace_check import ATTEMPTS_OFFSET
        card[ATTEMPTS_OFFSET + 1] = 2
        check_pin2_card(card, attempts=2)
        with self.assertRaises(ValueError):
            check_pin2_card(card)

    def test_change_and_restore_are_distinct_authenticated_commands(self):
        for restore in (False, True):
            check_trace(pin2_trace(restore), restore)
            with self.assertRaises(ValueError):
                check_trace(pin2_trace(restore), not restore)

    def test_reset_missing_contacts_or_failure_rejected(self):
        text = pin2_trace()
        for bad in (text + '\nnsb3_board_reset_request: data=05',
                    text.replace('nsb3_security_key:', 'missing_key:'),
                    text.replace('ins=24 sw=9000', 'ins=24 sw=9804')):
            with self.assertRaises(ValueError):
                check_trace(bad)

    def test_complete_card_comparison_protects_pin1_and_files(self):
        original = make_current_profile(True)
        check_pin2_card(original)
        changed = bytearray(original)
        changed[CHV_OFFSET + 8:CHV_OFFSET + 16] = padded_pin('8765')
        check_pin2_card(changed, True)
        for offset in (0, CHV_OFFSET, CHV_OFFSET + 16):
            bad = bytearray(changed)
            bad[offset] ^= 1
            with self.assertRaises(ValueError):
                check_pin2_card(bad, True)


if __name__ == '__main__':
    unittest.main()
