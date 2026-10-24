import unittest

from tools.nsb3_sms_check import check_trace, SAVE_KEYS, READ_KEYS, DELETE_KEYS
from tools.sim_sms_draft import DRAFT


def trace(phase='save'):
    lines = ['nsb3_phonecode_matched_branch: retry=00 observer_writes=0',
             'nsb3_phonecode_menu: down=1 physical_only=1',
             'nsb3_phonecode_menu: down=0 physical_only=1']
    lines += [f'name=Keypad {digit} down={state}' for digit in '12345' for state in (1, 0)]
    lines += [f'nsb3_sms_key: key={key} down={state} t=40.0 physical_only=1'
              for key in {'save': SAVE_KEYS, 'read': READ_KEYS, 'delete': DELETE_KEYS, 'empty': READ_KEYS[:4]}[phase]
              for state in (1, 0)]
    tx = bytes.fromhex('ff00ffa0a40000026f3c')
    if phase in ('read', 'empty'):
        tx += bytes.fromhex('a0b20104b0')
    else:
        tx += bytes.fromhex('a0dc0104b0') + (b'\0' + DRAFT[1:] if phase == 'delete' else DRAFT)
    lines += [f'nsb3_sim_wire: direction=TX data={value:02x} t=61.0 observer_writes=0' for value in tx]
    return '\n'.join(lines)


class Nsb3SmsTest(unittest.TestCase):
    def test_physical_save_and_read_only_cold_phase(self):
        check_trace(trace())
        check_trace(trace('read'), 'read')
        with self.assertRaises(ValueError):
            check_trace(trace(), 'read')
        with self.assertRaises(ValueError):
            check_trace(trace('read'))

    def test_delete_and_empty_require_their_own_contacts_and_wire_action(self):
        check_trace(trace('delete'), 'delete')
        check_trace(trace('empty'), 'empty')
        for phase, other in (('delete', 'save'), ('delete', 'read'), ('empty', 'delete')):
            with self.assertRaises(ValueError):
                check_trace(trace(other), phase)

    def test_missing_authentication_or_contacts_fail(self):
        for phase in ('save', 'read', 'delete', 'empty'):
            text = trace(phase)
            for bad in (text.replace('matched_branch', 'unmatched'),
                        text.replace('physical_only=1', 'physical_only=0'),
                        text.replace('nsb3_sms_key:', 'unobserved_sms_key:')):
                with self.assertRaises(ValueError):
                    check_trace(bad, phase)

    def test_saved_body_and_selected_file_must_match(self):
        text = trace()
        for bad in (text.rsplit('\n', 1)[0],
                    text.replace('data=3c t=', 'data=3a t='),
                    text.replace('data=41 t=', 'data=42 t=')):
            with self.assertRaises(ValueError):
                check_trace(bad)


if __name__ == '__main__':
    unittest.main()
