from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from tools.nsk1_puk2_exhaust import check_permanent_cold
from tools.nsk1_pin_change_roundtrip import MENU
from tools.sim_security_trace_check import ATTEMPTS_OFFSET


class PermanentPuk2Tests(unittest.TestCase):
    def trace(self):
        lines = ['SIM status ins=20 sw=9000 chv=3/0 puk=10/0 enabled=1']
        for phase, count in (('pin', 5), ('phone', 6)):
            for i in range(1, count+1):
                for edge in (1, 0):
                    lines.append(f'nsk1_pin_contact: phase={phase} index={i} down={edge} t=12 physical_only=1')
        menu = MENU[:]
        menu.insert(-1, 'down')
        for phase, keys in (('menu', menu), ('navigate', ['down'])):
            for i, key in enumerate(keys, 1):
                for edge in (1, 0):
                    lines.append(f'nsk1_pin_change_contact: phase={phase} index={i} key={key} down={edge} t=60 physical_only=1')
        decoded = [1, 2, 3, 4, 0x19, 1, 2, 3, 4, 5, 0x19]
        decoded += [0x19 if key == 'menu' else 0x18 for key in menu] + [0x18]
        lines += [f'nsk1_access_key: value={key:02x} t=60 observer_writes=0' for key in decoded]
        return '\n'.join(lines)

    def check(self, trace, mutate=False):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            storage = run / 'nvram/nsk1r4t'
            storage.mkdir(parents=True)
            before = bytearray(3570)
            before[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET+4] = bytes((3, 0, 10, 0))
            after = bytearray(before)
            if mutate:
                after[0] = 1
            (storage / 'sim_card').write_bytes(after)
            (run / 'initial_eeprom.bin').write_bytes(bytes(2048))
            with patch('tools.nsk1_puk2_exhaust.check_cold_link', return_value=bytes(before)), \
                 patch('tools.nsk1_puk2_exhaust.check_phase', return_value=({}, trace)), \
                 patch('tools.nsk1_puk2_exhaust.frame_check'):
                check_permanent_cold(run, Path('previous'), True)

    def test_navigation_requires_decoded_keys_and_unchanged_card(self):
        trace = self.trace()
        self.check(trace)
        with self.assertRaises(ValueError):
            self.check(trace, mutate=True)
        with self.assertRaises(ValueError):
            self.check(trace.rsplit('\n', 1)[0])

    def test_refusal_must_not_issue_card_mutation(self):
        with self.assertRaises(ValueError):
            self.check(self.trace() + '\nSIM status ins=2c sw=9840 chv=3/0 puk=10/0 enabled=1')


if __name__ == '__main__':
    unittest.main()
