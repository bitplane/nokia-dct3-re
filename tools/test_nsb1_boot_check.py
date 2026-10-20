import unittest

from tools.nsb1_boot_check import check_trace


class BootCheckTest(unittest.TestCase):
    program = b'\x12\x34' * 186

    def trace(self):
        lines = ['Soft reset', 'nsb1_program: words=186 data=' + self.program.hex() + ' observer_writes=0']
        lines += ['nsb1_dsp_release: command=01 arguments=0100030000008000000100010200 pc=0028f9c8 t=0.007852154 observer_writes=0',
                  'nsb1_dsp_release: command=01 arguments=0100030000008000000100010200 pc=0028f9c8 t=0.009274923 observer_writes=0',
                  'nsb1_dsp_result_read: address=00010004 data=ffff0000 mask=ffff0000 pc=0028f9dc t=0.008895923 observer_writes=0']
        for time in (0.1, 0.5, 1, 2, 5):
            lines.append(f'nsb1_boot: t={time:.9f} pc=002a4674 sp=0010c6dc irqmask=de '
                         'eeprom_mode=01 page=20 capacity=00002000 selector=00 dsp_family=0004 '
                         'catalogue0=002b35a4/002b94e4 shared_writes=246 observer_writes=0')
            lines.append(f'nsb1_interrupts: t={time:.9f} cpsr=2000003f ctrl=05')
        for time in (0.3, 0.6, 0.9):
            lines.append(f'nsb1_supervisor_flag: value=00 pc=002acaca t={time:.9f} observer_writes=0')
        return '\n'.join(lines)

    def test_complete(self):
        check_trace(self.trace(), self.program)

    def test_wrong_program(self):
        with self.assertRaisesRegex(ValueError, 'program'):
            check_trace(self.trace(), b'\x56\x78' * 186)

    def test_geometry_not_inherited(self):
        with self.assertRaises(ValueError):
            check_trace(self.trace().replace('capacity=00002000', 'capacity=00000800'), self.program)

    def test_missing_sample(self):
        with self.assertRaises(ValueError):
            check_trace(self.trace().replace('t=5.000000000', 't=6.000000000'), self.program)

    def test_changed_frontier(self):
        with self.assertRaises(ValueError):
            check_trace(self.trace().replace('pc=002a4674', 'pc=0028fc00'), self.program)

    def test_error_after_samples(self):
        with self.assertRaises(ValueError):
            check_trace(self.trace() + '[LUA ERROR]', self.program)

    def test_silent_fiq_rejected(self):
        with self.assertRaisesRegex(ValueError, 'wakeups'):
            check_trace(self.trace().replace('pc=002acaca', 'pc=0020010c'), self.program)

    def test_disabled_delivery_rejected(self):
        with self.assertRaisesRegex(ValueError, 'delivery'):
            check_trace(self.trace().replace('ctrl=05', 'ctrl=0a'), self.program)

    def test_observer_read_not_firmware_evidence(self):
        with self.assertRaisesRegex(ValueError, 'firmware-owned'):
            check_trace(self.trace().replace('pc=0028f9dc', 'pc=002a4674'), self.program)

    def test_wrong_release_arguments(self):
        with self.assertRaisesRegex(ValueError, 'release arguments'):
            check_trace(self.trace().replace('0100030000008000000100010200', '010003000000e800000100010200'), self.program)
