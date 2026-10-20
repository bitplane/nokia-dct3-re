from pathlib import Path
import tempfile
import unittest

from tools.nsb1_rom4_compare import check_lcd, check_trace, prepare


class NativeCompareTest(unittest.TestCase):
    program3 = b'\x12\x34' * 186
    program4 = b'\x56\x78' * 186

    def trace(self):
        lines = ['Soft reset']
        for index, program in enumerate((self.program3, self.program4), 1):
            lines.append('nsb1_dsp_release: command=01 arguments=0100030000008000000100010200 pc=0028f9c8')
            lines.append(f'nsb1_release_program: release={index} words=186 data={program.hex()} observer_writes=0')
            for data, mask, pc in (('00040000', 'ffff0000', '0028f9dc'),
                                   ('00040000', 'ffff0000', '0028fa08'),
                                   (f'{index + 2:08x}', '0000ffff', '0028fa0c')):
                lines.append(f'nsb1_dsp_result_read: address=00010004 data={data} mask={mask} pc={pc}')
        lines.append('nsb1_dsp_release: command=01 arguments=fd00ff8002440500007800000000 pc=0028fc50')
        for data in ('0000ffff', '00000004'):
            lines.append(f'nsb1_dsp_result_read: address=00010000 data={data} mask=0000ffff pc=0028fa96')
        for time in (0.5, 1, 2, 5):
            lines.append(f'nsb1_dsp_state: t={time:.9f} words=0a051eff00d00003 '
                         'flags=04ec/1074 released=1 init_success=01 saved_version=0004 '
                         'handoffs=63 releases=3 observer_writes=0')
        lines += ['rom4_port_write: port=02 data=ffff pc=0d8f',
                  'rom4_port_write: port=36 data=0080 pc=8d98']
        return '\n'.join(lines)

    def test_native_continuation(self):
        check_trace(self.trace(), self.program3, self.program4, 0)

    def test_wrong_own_program(self):
        with self.assertRaisesRegex(ValueError, 'own uploads'):
            check_trace(self.trace(), self.program4, self.program3, 0)

    def test_wrong_version_sequence(self):
        with self.assertRaisesRegex(ValueError, 'mismatch'):
            check_trace(self.trace().replace('data=00000003', 'data=00000004'), self.program3, self.program4, 0)

    def test_no_block_completion(self):
        with self.assertRaisesRegex(ValueError, '63-handoff'):
            check_trace(self.trace().replace('handoffs=63', 'handoffs=62'), self.program3, self.program4, 0)

    def test_native_execution_required(self):
        with self.assertRaisesRegex(ValueError, 'execution'):
            check_trace(self.trace().replace('pc=8d98', 'pc=8d99'), self.program3, self.program4, 0)

    def test_final_publication_required(self):
        with self.assertRaisesRegex(ValueError, 'final-result'):
            check_trace(self.trace().replace('data=00000004 mask=0000ffff pc=0028fa96',
                                            'data=0000ffff mask=0000ffff pc=0028fa96'), self.program3, self.program4, 0)

    def test_crash_and_reset_rejected(self):
        for trace, code in ((self.trace(), 139), (self.trace() + '\nSoft reset', 0)):
            with self.subTest(code=code), self.assertRaises(ValueError):
                check_trace(trace, self.program3, self.program4, code)

    def test_prior_storage_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, 'prior EEPROM'):
                prepare(Path(directory), Path(directory))

    def test_unqualified_inputs_leave_no_run(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'noki5190').mkdir()
            (root / 'noki5190/5190f671a.fls').write_bytes(b'not the own image')
            with self.assertRaisesRegex(ValueError, 'unqualified'):
                prepare(root, root / 'run')
            self.assertFalse((root / 'run').exists())


class LcdCompareTest(unittest.TestCase):
    def trace(self):
        commands = '24 40 80 41 80 42 80 43 80 44 80 45 80 20 21 05 14 ce 20 0c'.split()
        lines = [f'nsb1_lcd_command: byte={value} pc=002ae138' for value in commands]
        for time, command, data, nonzero in ((0.1, 0, 0, 0), (0.5, 32, 1008, 504),
                                            (1, 32, 1008, 504), (2, 68, 2016, 504),
                                            (5, 68, 2016, 504)):
            lines.append(f'nsb1_lcd_summary: t={time} commands={command} data_bytes={data} '
                         f'nonzero={nonzero} observer_writes=0')
        return '\n'.join(lines)

    def test_bus_and_both_pixel_states(self):
        from PIL import Image
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            Image.new('RGB', (84, 48), 'black').save(root / 'nsb1_native_1s.png')
            Image.new('RGB', (84, 48), 'white').save(root / 'nsb1_native_5s.png')
            check_lcd(self.trace(), root)
            Image.new('RGB', (84, 48), 'white').save(root / 'nsb1_native_1s.png')
            with self.assertRaisesRegex(ValueError, 'pixels'):
                check_lcd(self.trace(), root)

    def test_command_drift(self):
        with self.assertRaisesRegex(ValueError, 'initialization'):
            check_lcd(self.trace().replace('byte=24', 'byte=25'), Path('.'))

    def test_missing_nonzero_transfer(self):
        with self.assertRaisesRegex(ValueError, 'traffic'):
            check_lcd(self.trace().replace('nonzero=504', 'nonzero=0'), Path('.'))
