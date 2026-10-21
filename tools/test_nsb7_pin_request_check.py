"""Physical request-setting commands and full cold card comparisons."""
import unittest

from tools.nsb7_pin_request_check import check_request, check_cards
from tools.nsb7_pin_check import serialized_profile
from tools.sim_security_trace_check import ENABLED_OFFSET


def fixture(enable):
    steps = [] if enable else [(16+i, 'Keypad '+digit) for i, digit in enumerate('12345')] + [(21, 'Left Softkey / Menu')]
    steps += [(26, 'Left Softkey / Menu'), (28, 'Keypad 4'), (30, 'Keypad 6'),
              (32, 'Keypad 1'), (35, 'Left Softkey / Menu'), (38, 'Keypad 1'),
              (39, 'Keypad 2'), (40, 'Keypad 3'), (41, 'Keypad 4'),
              (42, 'Left Softkey / Menu'), (45, 'Scroll Up' if enable else 'Scroll Down'),
              (48, 'Left Softkey / Menu')]
    ins = 0x28 if enable else 0x26
    lines = [] if enable else ['SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1']
    for second, key in steps:
        code = int(key[-1]) if key.startswith('Keypad ') else {'Left Softkey / Menu': 0x19, 'Scroll Up': 0x17, 'Scroll Down': 0x18}[key]
        lines += [f'nsb7_request_input: enable={int(enable)} key={key} state=1 t={second}.000000000',
                  f'nsb7_key_decode: key={code:02x} observer_writes=0']
        if second == 48:
            apdu = bytes((0xa0, ins, 0, 1, 8)) + b'1234\xff\xff\xff\xff'
            lines += [f'nsb7_sim_tx: index={i} value={byte:02x}' for i, byte in enumerate(apdu, 1)]
            lines.append(f'SIM status ins={ins:02x} sw=9000 chv=3/3 puk=10/10 enabled={int(enable)}')
        lines.append(f'nsb7_request_input: enable={int(enable)} key={key} state=0 t={second}.200000000')
    return '\n'.join(lines + ['nsb7_request_input: complete'])


class RequestCheckTest(unittest.TestCase):
    def test_off_and_on(self):
        check_request(fixture(False), False)
        check_request(fixture(True), True)

    def test_wrong_key_status_or_credential(self):
        for old, new in [('key=Keypad 6', 'key=Keypad 5'), ('key=18 observer_writes=0', 'key=17 observer_writes=0'),
                         ('ins=26 sw=9000', 'ins=26 sw=9804'), ('value=31', 'value=39'),
                         ('nsb7_request_input: complete', '')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_request(fixture(False).replace(old, new), False)

    def test_disabled_card_must_bypass_startup_verify(self):
        with self.assertRaises(ValueError):
            check_request(fixture(True) + '\nSIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1', True)

    def test_exact_cold_card_chain(self):
        enabled = serialized_profile()
        disabled = bytearray(enabled)
        disabled[ENABLED_OFFSET] = 0
        check_cards([(enabled, bytes(disabled)), (bytes(disabled), enabled), (enabled, enabled)])

    def test_unrelated_card_mutation(self):
        enabled = serialized_profile()
        disabled = bytearray(enabled)
        disabled[ENABLED_OFFSET] = 0
        altered = b'\x00' + enabled[1:]
        with self.assertRaises(ValueError):
            check_cards([(enabled, bytes(disabled)), (bytes(disabled), altered), (altered, altered)])


if __name__ == '__main__':
    unittest.main()
