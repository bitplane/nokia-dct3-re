import unittest

from tools.nsm4_native_boundary_check import check_trace


class Nsm4NativeTest(unittest.TestCase):
    def fixture(self):
        marker = 'address=861f pc=8620 stage=verifier'
        return ('Fatal error: Staged DSP needs unavailable program word: ' + marker,
                'staged_dsp: release entry=0f00 words=246 prom_input=0006 clock=13000000 stage=verifier\n'
                'staged_dsp: readonly_program_write address=ff87 data=0006 pc=0f12\n'
                'staged_dsp: unavailable_program ' + marker)

    def test_precise_negative_boundary(self):
        output, trace = self.fixture()
        check_trace(output, trace, 1)
        for old, new in [('words=246', 'words=247'), ('prom_input=0006', 'prom_input=0004'),
                         ('address=ff87', 'address=ff88'), ('address=861f', 'address=8620')]:
            with self.assertRaises(ValueError):
                check_trace(output, trace.replace(old, new), 1)
        for code in (0, -9, 124):
            with self.assertRaises(ValueError):
                check_trace(output, trace, code)
        for marker in ('staged_dsp: publication', 'runtime_hle_handoff', 'loader2_verified'):
            with self.assertRaises(ValueError):
                check_trace(output, trace + '\n' + marker, 1)


if __name__ == '__main__':
    unittest.main()
