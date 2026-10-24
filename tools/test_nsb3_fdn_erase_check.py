import unittest

from tools.nsb3_fdn_erase_check import check_phase, CANCEL_KEYS, READ_KEYS, ERASE_KEYS, EMPTY_KEYS
from tools.test_nsb3_pin_request_check import trace
from tools.sim_security_trace_check import padded_pin


def phase_trace(phase):
    keys = (CANCEL_KEYS, READ_KEYS, ERASE_KEYS, EMPTY_KEYS)[phase]
    text = trace(3) + '\n' + '\n'.join(
        f'nsb3_security_key: key={key} down={state} t=66.0 physical_only=1'
        for key in keys for state in (1, 0))
    tx = bytes.fromhex('a0a40000026f7ea044000000a0a40000026f07a044000000a0a40000026f3ba0b2010420')
    text += '\n' + '\n'.join(['SIM status ins=44 sw=9000 chv=3/3 puk=10/10 enabled=1'] * 2)
    if phase in (0, 2):
        tx += bytes.fromhex('a020000208') + padded_pin('5678')
        text += '\nSIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1'
    if phase == 2:
        tx += bytes.fromhex('a0dc010420') + b'\xff' * 32
        text += '\nSIM status ins=dc sw=9000 chv=3/3 puk=10/10 enabled=1'
    return text + '\n' + '\n'.join(
        f'nsb3_sim_wire: direction=TX data={byte:02x} t=70.0 observer_writes=0' for byte in tx)


class Nsb3FdnEraseTest(unittest.TestCase):
    def test_four_phases_have_distinct_contacts_authentication_and_writes(self):
        for phase in range(4):
            check_phase(phase_trace(phase), phase)
            with self.assertRaises(ValueError):
                check_phase(phase_trace(phase), (phase + 1) % 4)

    def test_cancellation_cannot_hide_a_record_write(self):
        tx = bytes.fromhex('a0dc010420') + b'\xff' * 32
        text = phase_trace(0) + '\n' + '\n'.join(
            f'nsb3_sim_wire: direction=TX data={byte:02x} t=71.0 observer_writes=0' for byte in tx)
        with self.assertRaises(ValueError):
            check_phase(text, 0)

    def test_bad_erase_status_and_reset_fail(self):
        for text in (phase_trace(2).replace('ins=dc sw=9000', 'ins=dc sw=9804'),
                     phase_trace(2) + '\nnsb3_board_reset_request: data=05'):
            with self.assertRaises(ValueError):
                check_phase(text, 2)


if __name__ == '__main__':
    unittest.main()
