import hashlib
from pathlib import Path
import unittest

from tools.c54x_vendor_operand_check import check, WORDS, WARNINGS, SOURCE_SHA256


class VendorOperandTests(unittest.TestCase):
    def listing(self):
        rows = ['TMS320C54x Assembler PC v4.2.0 fixture']
        rows += [f'       {n+1} {n:06x} {word:04X} instruction' for n, word in enumerate(WORDS)]
        rows += [f' "operand_probe.asm", WARNING! at line {n}: [W0000] *+ARn addressing is for write-only'
                 for n in WARNINGS]
        rows += ['No Assembly Errors, 5 Assembly Warnings']
        return '\n'.join(rows)

    def test_complete_probe(self):
        result = check(self.listing())
        self.assertTrue(result['short_rmw_flagged'])
        self.assertFalse(result['dsp_execution_validated'])
        self.assertFalse(result['silicon_trap_validated'])

    def test_source_identity(self):
        source = Path(__file__).resolve().parents[1] / 'fixtures/c54x_vendor_operands/operand_probe.asm'
        self.assertEqual(hashlib.sha256(source.read_bytes()).hexdigest(), SOURCE_SHA256)

    def test_each_warning_required(self):
        for n in WARNINGS:
            with self.subTest(line=n):
                text = self.listing().replace(f'WARNING! at line {n}:', 'omitted:')
                with self.assertRaisesRegex(ValueError, 'diagnostics'):
                    check(text)

    def test_unflagged_control_cannot_become_warning(self):
        text = self.listing() + '\n "operand_probe.asm", WARNING! at line 5: [W0000] *+ARn addressing is for write-only'
        with self.assertRaisesRegex(ValueError, 'diagnostics'):
            check(text)

    def test_encoding_and_addresses_pinned(self):
        for before, after in [('109A', '1092'), ('000001', '000000')]:
            with self.subTest(before=before):
                with self.assertRaises(ValueError):
                    check(self.listing().replace(before, after, 1))

    def test_version_and_terminal_required(self):
        for text in [self.listing().replace('v4.2.0', 'v4.1.0'),
                     self.listing().replace('No Assembly Errors', 'Assembly failed')]:
            with self.assertRaises(ValueError):
                check(text)


if __name__ == '__main__':
    unittest.main()
