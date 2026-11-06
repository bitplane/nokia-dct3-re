"""Synthetic capture controls require no proprietary ROM bytes."""
import unittest
from pathlib import Path
import tempfile

from tools.dsp_rom6_candidate_check import ALIAS, SWEEP, audit, merge_captures, parse_rows, reconstruct


class Rom6CandidateTests(unittest.TestCase):
    def test_hash_mismatch_and_unbounded_input_fail_before_reconstruction(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / 'input'
            for data, message in ((b'changed', 'hash mismatch'), (b'', 'input size')):
                path.write_bytes(data)
                with self.assertRaisesRegex(ValueError, message):
                    audit(root, {'input': '0'*64})

    def test_capture_status_duplicate_and_extent_rules(self):
        self.assertEqual(parse_rows(b'8000 1234 ok\n8001 abcd ok-confirmed3x\n', SWEEP, 0x8000, 0xafff),
                         {0x8000: 0x1234, 0x8001: 0xabcd})
        for text in (b'8000 1234 TIMEOUT', b'8000 1234 guessed', b'8000 1234 ok\n8000 1234 ok',
                     b'7fff 1234 ok', b'8000 12345 ok', b'8000 1234 ok trailing'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                parse_rows(text, SWEEP, 0x8000, 0xafff)

    def test_alias_has_its_own_format_and_bounds(self):
        self.assertEqual(parse_rows(b'    b000:       2938\n', ALIAS, 0xb000, 0xffff), {0xb000: 0x2938})
        with self.assertRaises(ValueError):
            parse_rows(b'    a000:       2938\n', ALIAS, 0xb000, 0xffff)

    def test_overlap_requires_agreement_and_ram_is_not_rom(self):
        rows = {0x3800: 0x1234}
        merge_captures(rows, {0x3800: 0x1234, 0x100: 0xffff})
        self.assertNotIn(0x100, rows)
        with self.assertRaisesRegex(ValueError, 'contradictory'):
            merge_captures(rows, {0x3800: 0xabcd})

    def test_program_data_padding_and_incomplete_capture(self):
        upper = bytes.fromhex('1234') * 0x5000
        base = bytes(0x16000) + upper
        low = dict.fromkeys(range(0x8000, 0xb000), 0x5678)
        blocks = dict.fromkeys([*range(0x80), *range(0x2000, 0x8000)], 0x9abc)
        subprobe = {a: blocks[a] for a in range(0x3800, 0x3c00)}
        alias = dict.fromkeys(range(0xb000, 0x10000), 0xdef0)
        program, data = reconstruct(base, upper, low, blocks, subprobe, alias)
        self.assertEqual(program[0x100:0x4000], bytes(0x3f00))
        self.assertEqual(program[0x10000:0x10002], bytes.fromhex('5678'))
        self.assertEqual(program[0x16000:0x16002], bytes.fromhex('1234'))
        self.assertEqual(data[0x16000:0x16002], bytes.fromhex('def0'))
        self.assertEqual(data[:0x16000], bytes(0x16000))
        del low[0x8000]
        with self.assertRaisesRegex(ValueError, 'coverage'):
            reconstruct(base, upper, low, blocks, subprobe, alias)


if __name__ == '__main__':
    unittest.main()
