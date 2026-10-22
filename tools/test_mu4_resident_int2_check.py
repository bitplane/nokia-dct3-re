import unittest
from pathlib import Path
import re
from unittest.mock import patch

from tools.mu4_resident_int2_check import SITES, check_branch


def fixture(pulse):
    lines = []
    def row(prefix, **fields):
        fields['observer_writes'] = 0
        lines.append(prefix + ': ' + ' '.join(f'{key}={value}' for key, value in fields.items()))
    for phase, time in [('before', 11), ('released', 11.05), ('after', 11.2)]:
        awake = pulse and phase != 'before'
        row('mu4_int2_state', phase=phase, t=time, pc='0298ea' if awake else '028923',
            idle=1, imr='0ac5' if awake else '0004', ifr='0438' if awake else '0c38', st1='6113', illegal=0)
    kinds = ['enter', 'resume', 'restore_mask', 'caller', 'enter'] if pulse else ['enter']
    for index, kind in enumerate(kinds, 1):
        pc, opcode = SITES[kind]
        row('mu4_int2_path', ordinal=index, kind=kind, pc=pc, opcode=opcode,
            imr='0ac5', ifr='0808' if kind == 'restore_mask' else '0008',
            identity='Execute_entry', operands='context_only')
    row('mu4_int2_paths_terminal', events=len(kinds), cap=32)
    writes = []
    if pulse:
        writes = [('0000', '0004', '0000'), ('0000', '0000', '0004'),
                  ('0001', '0c38', '0438'), ('0000', '0004', '0ac5')]
        writes += [('0000', '0ac5', '0000'), ('0000', '0000', '0ac5')] * 127
        lines += ['mu4_mcu_int2_input: asserted=1 net=UC_DSP_INT cpu_irq=2 t=11.016666667 source=bench_pin firmware_state_forcing=0',
                  'mu4_mcu_int2_input: asserted=0 net=UC_DSP_INT cpu_irq=2 t=11.066666667 source=bench_pin firmware_state_forcing=0']
    for index, (address, before, value) in enumerate(writes):
        row('mu4_int2_register_write', index=index, t=11.02, address=address, before=before,
            value=value, mask='ffff', identity='data_write', operands='context_only')
    row('mu4_int2_writes_terminal', events=len(writes), cap=1024)
    lines.append(f'mu4_int2_fixture: complete=1 pulse_asserted={int(pulse)} declared_pulse_ms=50 normal_mcu_waveform=unproved firmware_state_forcing=0')
    return '\n'.join(lines)


class ResidentInt2Tests(unittest.TestCase):
    def test_debugger_hex_cap_matches_declared_decimal_cap(self):
        source=Path(__file__).with_name('mu4_resident_int2_observe.lua').read_text()
        limit=re.search(r"temp1<([0-9a-f]+) &&",source)
        self.assertIsNotNone(limit)
        self.assertEqual(int(limit[1],16),32)

    def check(self, text, pulse):
        with patch('tools.mu4_resident_int2_check.serial_check') as serial:
            result = check_branch(text, 'console', pulse)
            serial.assert_called_once_with(text, 'console')
            return result

    def test_both_branches(self):
        self.check(fixture(False), False)
        self.check(fixture(True), True)

    def test_reject_scope_and_register_mutations(self):
        original = fixture(True)
        for old, new in [('value=0438', 'value=0c38'), ('ifr=0808', 'ifr=0000'),
                         ('cap=1024', 'cap=258'), ('observer_writes=0', 'observer_writes=1'),
                         ('normal_mcu_waveform=unproved', 'normal_mcu_waveform=proved'),
                         ('t=11.066666667', 't=11.076666667'), ('opcode=8a01', 'opcode=8a02'),
                         ('identity=data_write', 'identity=Execute_entry')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                self.check(original.replace(old, new), True)

    def test_control_cannot_claim_wake(self):
        with self.assertRaises(ValueError):
            self.check(fixture(True), False)


if __name__ == '__main__':
    unittest.main()
