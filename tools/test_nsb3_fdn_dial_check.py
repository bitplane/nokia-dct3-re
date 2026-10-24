import unittest

from tools.nsb3_fdn_dial_check import check_dial, card_expected
from tools.test_nsb3_pin_request_check import trace


def wire_bytes(data):
    return '\n' + '\n'.join(
        f'nsb3_sim_wire: direction=TX data={byte:02x} t=48.1 observer_writes=0'
        for byte in data)


def dial_trace(digits='12346', enabled=True, post_read=True):
    text = trace(3)
    if enabled:
        text += wire_bytes(bytes.fromhex('a0a40000026f7ea044000000a0a40000026f07a044000000'))
        text += '\n' + '\n'.join(['SIM status ins=44 sw=9000 chv=3/3 puk=10/10 enabled=1'] * 2)
    text += wire_bytes(bytes.fromhex('a0a40000026f3ba0b2010420'))
    keys = ('Right Softkey / C',) + tuple('Keypad ' + d for d in digits) + ('Send',)
    for key, when in zip(keys, (40, 42, 43, 44, 45, 46, 48)):
        for state in (1, 0):
            text += f'\nnsb3_security_key: key={key} down={state} t={when + (0 if state else .15):.9f} physical_only=1'
    if post_read:
        text += wire_bytes(bytes.fromhex('a0b2010420'))
    return text


class Nsb3FdnDialTest(unittest.TestCase):
    def test_enabled_listed_and_unlisted_and_disabled_controls(self):
        for digits, enabled in (('12346', True), ('54321', True), ('54321', False)):
            check_dial(dial_trace(digits, enabled), digits, enabled)

    def test_listed_lookup_must_follow_send_with_selected_file_context(self):
        with self.assertRaises(ValueError):
            check_dial(dial_trace(post_read=False), '12346', True)
        check_dial(dial_trace(), '12346', True)

    def test_wrong_keys_cadence_mutation_and_reset_fail(self):
        good = dial_trace()
        for bad in (good.replace('key=Send ', 'key=End '),
                    good.replace('t=48.000000000', 't=47.000000000'),
                    good + wire_bytes(bytes.fromhex('a0dc010420') + b'\xff' * 32),
                    good + '\nnsb3_board_reset_request: data=05'):
            with self.assertRaises(ValueError):
                check_dial(bad, '12346', True)

    def test_off_only_changes_validity(self):
        on, off = card_expected(True), card_expected(False)
        self.assertEqual([i for i, pair in enumerate(zip(on, off)) if pair[0] != pair[1]], [3575])


if __name__ == '__main__':
    unittest.main()
