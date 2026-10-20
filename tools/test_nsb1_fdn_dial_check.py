import unittest
from unittest.mock import patch
from tools.nsb1_fdn_dial_check import check_dial


class Nsb1FdnDialTests(unittest.TestCase):
    def trace(self):
        lines = ['SIM status ins=20 sw=9000']
        for phase, count in (('pin', 5), ('phone', 6)):
            for i in range(1, count + 1):
                for down in (1, 0):
                    lines.append(f'nsb1_pin_contact: phase={phase} index={i} down={down} t=12 physical_only=1')
        for i in range(1, 7):
            for down in (1, 0):
                lines.append(f'nsb1_fdn_contact: index={i} down={down} t=30 physical_only=1')
        return '\n'.join(lines)

    def test_protocol_phase_matches_persistent_state(self):
        for enabled, phase in ((True, 1), (False, 3)):
            with patch('tools.nsb1_fdn_dial_check.check_phase') as protocol:
                trace = self.trace()
                check_dial(trace, enabled)
                protocol.assert_called_once_with(trace, phase, 'nsb1_sim_tx')

    def test_missing_release_is_rejected(self):
        with self.assertRaises(ValueError):
            check_dial(self.trace().replace('nsb1_fdn_contact: index=6 down=0 ', 'missing '), True)

    def test_dial_before_phone_submission_is_rejected(self):
        trace = self.trace().splitlines()
        trace.insert(0, trace.pop(-12))
        with self.assertRaises(ValueError):
            check_dial('\n'.join(trace), True)


if __name__ == '__main__':
    unittest.main()
