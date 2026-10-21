"""Ordinary CHANGE CHV rejection, credential-only writes and cold reuse."""
import unittest

from tools.nsb7_pin_change_check import check_change, check_cards
from tools.nsb7_pin_check import serialized_profile
from tools.sim_security_trace_check import CHV_OFFSET, ATTEMPTS_OFFSET, padded_pin
from tools.test_nsb7_pin_check import fixture as pin_fixture


def fixture(wrong, pin2=False, old=None, new=None, unblock=False):
    old = old if old is not None else (('87654322' if wrong else '87654321') if unblock else
                                      '1111' if wrong else '5678' if pin2 else '1234')
    new = new if new is not None else ('8765' if pin2 else '4321')
    steps = [(16+i, 'Keypad '+digit, 'phone') for i, digit in enumerate('12345')]
    steps += [(21, 'Left Softkey / Menu', 'phone'), (26, 'Left Softkey / Menu', 'menu'),
              (28, 'Keypad 4', 'menu'), (30, 'Keypad 6', 'menu'), (32, 'Keypad 5', 'menu')]
    if pin2:
        steps.append((34, 'Keypad 2', 'menu'))
    steps.append((35, 'Left Softkey / Menu', 'menu'))
    new_start = 38+len(old)+3
    final_confirm = new_start+len(new)+3+len(new)
    for phase, digits, start in [('puk2' if unblock else 'old', old, 38),
                                 ('new', new, new_start), ('confirm', new, new_start+len(new)+3)]:
        steps += [(start+i, 'Keypad '+digit, phase) for i, digit in enumerate(digits)]
        steps.append((start+len(digits), 'Left Softkey / Menu', phase))
    lines = [pin_fixture(True)]
    for second, key, phase in steps:
        code = 0x19 if key == 'Left Softkey / Menu' else int(key[-1])
        lines += [f'nsb7_change_input: phase={phase} key={key} state=1 t={second}.000000000',
                  f'nsb7_key_decode: key={code:02x} observer_writes=0']
        if second == final_confirm:
            instruction = 0x2c if unblock else 0x24
            apdu = bytes((0xa0, instruction, 0, 2 if pin2 else 1, 16)) + padded_pin(old) + padded_pin(new)
            lines += [f'nsb7_sim_tx: index={i} value={byte:02x}' for i, byte in enumerate(apdu, 1)]
            remaining = 0 if unblock and wrong else 2 if pin2 and wrong else 3
            lines.append(f'SIM status ins={instruction:02x} sw={"9804" if wrong else "9000"} chv={3 if pin2 else 2 if wrong else 3}/{remaining} puk=10/{9 if unblock and wrong else 10} enabled=1')
            if unblock and not wrong:
                automatic = bytes.fromhex('a020000208') + padded_pin(new)
                lines += [f'nsb7_sim_tx: index={i} value={byte:02x}' for i, byte in enumerate(automatic, 30)]
                lines.append('SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1')
        lines.append(f'nsb7_change_input: phase={phase} key={key} state=0 t={second}.200000000')
    return '\n'.join(lines + ['nsb7_change_input: complete'])


class PinChangeTest(unittest.TestCase):
    def test_both_outcomes(self):
        check_change(fixture(True), True)
        check_change(fixture(False), False)

    def test_wrong_or_missing_evidence(self):
        for old, new in [('phase=confirm', 'phase=missing'), ('key=Keypad 5 state=1 t=32.', 'key=Keypad 6 state=1 t=32.'),
                         ('ins=24 sw=9000', 'ins=24 sw=9804'), ('value=34', 'value=39'),
                         ('nsb7_change_input: complete', '')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_change(fixture(False).replace(old, new), False)

    def test_complete_card_comparisons(self):
        initial = serialized_profile()
        wrong = bytearray(initial)
        wrong[ATTEMPTS_OFFSET] = 2
        changed = bytearray(initial)
        changed[CHV_OFFSET:CHV_OFFSET+8] = padded_pin('4321')
        check_cards([(initial, bytes(wrong)), (initial, bytes(changed)), (bytes(changed), bytes(changed))])

    def test_unrelated_mutation(self):
        initial = serialized_profile()
        wrong = bytearray(initial)
        wrong[ATTEMPTS_OFFSET] = 2
        changed = bytearray(initial)
        changed[CHV_OFFSET:CHV_OFFSET+8] = padded_pin('4321')
        changed[0] = 0
        with self.assertRaises(ValueError):
            check_cards([(initial, bytes(wrong)), (initial, bytes(changed)), (bytes(changed), bytes(changed))])


if __name__ == '__main__':
    unittest.main()
