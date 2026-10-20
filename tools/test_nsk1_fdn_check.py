import unittest
from unittest.mock import patch
from tools.nsk1_fdn_check import check_trace, phase_keys, KEY_CODES


class Nsk1FdnTests(unittest.TestCase):
    def trace(self, phase):
        lines = ['SIM status ins=20 sw=9000']
        for name, count in (('pin', 5), ('phone', 6)):
            for index in range(1, count+1):
                for edge in (1, 0):
                    lines.append(f'nsk1_pin_contact: phase={name} index={index} down={edge} t=24 physical_only=1')
        keys = phase_keys(phase)
        for index, key in enumerate(keys, 1):
            for edge in (1, 0):
                lines.append(f'nsk1_fdn_contact: index={index} key={key} down={edge} t=34 physical_only=1')
        decoded = [1, 2, 3, 4, 0x19, 1, 2, 3, 4, 5, 0x19] + [KEY_CODES[key] for key in keys]
        lines += [f'nsk1_fdn_key: value={key:02x} t=34 observer_writes=0' for key in decoded]
        return '\n'.join(lines)

    def test_each_phase_uses_shared_protocol_and_own_physical_route(self):
        for phase in range(4):
            trace = self.trace(phase)
            with patch('tools.nsk1_fdn_check.check_phase') as protocol:
                check_trace(trace, phase)
                protocol.assert_called_once_with(trace, phase, 'nsk1_sim_tx')
        with self.assertRaises(ValueError):
            phase_keys(4)

    def test_missing_key_or_duplicate_contact_is_rejected(self):
        trace = self.trace(0)
        for bad in (trace.rsplit('\n', 1)[0], trace + '\nnsk1_fdn_contact: malformed'):
            with self.assertRaises(ValueError):
                check_trace(bad, 0)


if __name__ == '__main__':
    unittest.main()
