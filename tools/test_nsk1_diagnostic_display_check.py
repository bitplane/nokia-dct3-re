import hashlib
import unittest
from unittest.mock import patch

from tools import nsk1_diagnostic_display_check as checker


def trace(cold=False):
    lines = [f'nsk1_code_contact: index={i} column={column} mask={mask:02x} down={down} t={11+i+delay:.9f} physical_only=1'
             for i, (column, mask) in enumerate(((2,2),(3,2),(4,2),(2,4),(3,4),(1,4)), 1)
             for down, delay in ((1,0),(0,.2))]
    lines.append('nsk1_code_gate: match=00000001 retry=00 cycles=0000000000000001 observer_writes=0')
    codes = [1,2,3,4,5,0x19]
    if not cold:
        lines += [f'nsk1_diagnostic_key: key={key} down={down} t={time+delay:.9f} physical_only=1'
                  for key, time in zip(checker.ACTIONS, checker.TIMES)
                  for down, delay in ((1,0),(0,.2))]
        codes += [0x19,0x17,0x19,0x19,0xa,0xa,0x19,0x1a,0x1a]
    lines += [f'nsk1_code_key: value={code:02x} t=1 observer_writes=0' for code in codes]
    return '\n'.join(lines)


class Nsk1DiagnosticDisplayTest(unittest.TestCase):
    def test_selection_and_authentication_only_cold_input_are_distinct(self):
        checker.check_authentication(trace())
        checker.check_authentication(trace(True), True)
        for text, cold in ((trace(), True), (trace(True), False)):
            with self.assertRaises(ValueError):
                checker.check_authentication(text, cold)

    def test_warning_acknowledgement_order_authentication_and_decoding_are_required(self):
        text = trace()
        for bad in (text.replace('key=menu down=1 t=34.', 'key=up down=1 t=34.'),
                    text.replace('t=39.000000000', 't=40.000000000'),
                    text.replace('match=00000001', 'match=00000000'),
                    text.replace('retry=00', 'retry=ff'),
                    text.replace('value=17 ', 'value=18 '),
                    text + '\nnsk1_code_key: malformed',
                    text + '\nnsk1_code_gate: malformed',
                    text + '\nnsk1_diagnostic_key: malformed',
                    text + '\nnsk1_restart_request:',
                    text + '\nillegal instruction'):
            with self.subTest(tail=bad[-100:]), self.assertRaises(ValueError):
                checker.check_authentication(bad)

    def test_whole_bank_not_only_selector_is_guarded(self):
        original = bytearray(2048)
        original[0x299] = 0xff
        selected = bytearray(original)
        selected[0x299], selected[0x500] = 0x3d, 0x7f
        with patch.object(checker, 'SOURCE', hashlib.sha256(original).hexdigest()), \
                patch.object(checker, 'SELECTED', hashlib.sha256(selected).hexdigest()):
            checker.check_bank(original)
            checker.check_bank(selected, True)
            for offset in (0, 0x299, 0x500, 2047):
                changed = bytearray(selected)
                changed[offset] ^= 1
                with self.subTest(offset=offset), self.assertRaises(ValueError):
                    checker.check_bank(changed, True)
            with self.assertRaises(ValueError):
                checker.check_bank(selected[:-1], True)


if __name__ == '__main__':
    unittest.main()
