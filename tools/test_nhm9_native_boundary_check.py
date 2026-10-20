import struct
import unittest
from unittest.mock import patch

from tools.nhm9_native_boundary_check import check


class NativeBoundaryTest(unittest.TestCase):
    output = 'address=861f pc=8620 stage=verifier'
    trace = ('staged_dsp: release entry=0f00 words=247 prom_input=0006 clock=13000000 stage=verifier\n'
             'staged_dsp: readonly_program_write address=ff87 data=0006 pc=0f12\n'
             'staged_dsp: unavailable_program address=861f pc=8620 stage=verifier\n')

    def verify(self, trace=None, code=1, target=0x861f):
        program = bytearray(494)
        struct.pack_into('>HH', program, 0x1ae, 0xf074, target)
        with patch('tools.nhm9_native_boundary_check.extract', return_value=program), \
                patch('tools.nhm9_native_boundary_check.extract_program_fragment', return_value=b''):
            check(b'', self.output, self.trace if trace is None else trace, code)

    def test_exact_boundary(self):
        self.verify()

    def test_wrong_call(self):
        with self.assertRaisesRegex(ValueError, 'CALL'):
            self.verify(target=0x861e)

    def test_crash_rejected(self):
        with self.assertRaisesRegex(ValueError, 'controlled'):
            self.verify(code=139)

    def test_wrong_program_extent(self):
        with self.assertRaisesRegex(ValueError, '247-word'):
            self.verify(self.trace.replace('words=247', 'words=223'))

    def test_missing_execution(self):
        with self.assertRaisesRegex(ValueError, 'unavailable-code'):
            self.verify(self.trace.replace('address=861f', 'address=861e'))

    def test_fabricated_completion(self):
        for marker in ('staged_dsp: publication', 'runtime_hle_handoff', 'loader2_verified'):
            with self.subTest(marker=marker), self.assertRaisesRegex(ValueError, 'unexpected completion'):
                self.verify(self.trace + marker)
