from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest

from tools.run_efr_filter_replay import compare, check_history


class FilterComparisonTests(unittest.TestCase):
    def test_exact_samples_and_history_update(self):
        expected = [tuple(range(40)), tuple(range(40, 80))]
        data = b''.join(struct.pack('<50H', *(row + row[-10:])) for row in expected)
        result = compare(data, expected)
        self.assertTrue(result['reference_equal'])
        self.assertEqual(result['samples'], 80)
        self.assertFalse(result['full_decoder_conformance'])
        with self.assertRaisesRegex(ValueError, 'incomplete'):
            compare(data[:-2], expected)
        with self.assertRaisesRegex(ValueError, 'history update'):
            compare(data[:-2] + b'\x00\x00', expected)
        for invalid in ([], [tuple(range(39))]):
            with self.assertRaisesRegex(ValueError, 'complete native'):
                compare(b'', invalid)

    def test_arithmetic_difference_is_characterized_not_hidden(self):
        expected = [tuple(range(40))]
        row = tuple(range(40))
        changed = (99,) + row[1:]
        result = compare(struct.pack('<50H', *(changed + changed[-10:])), expected)
        self.assertFalse(result['reference_equal'])
        self.assertEqual(result['differences'], [dict(frame=0, subframe=0, samples=1)])

    def test_history_chain_includes_frame_boundaries(self):
        frames = [dict(RAW=list(range(frame * 160, (frame + 1) * 160))) for frame in range(2)]
        records = {}
        for index in range(8):
            records[index // 4, index % 4, 'MEM'] = tuple(range(index * 40 - 10, index * 40))
        check_history(records, frames)
        records[1, 0, 'MEM'] = (0,) * 10
        with self.assertRaisesRegex(ValueError, 'preceding output tail'):
            check_history(records, frames)


@unittest.skipUnless(shutil.which('gcc'), 'requires a C compiler for the I/O harness')
class FilterHarnessTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory()
        cls.root = Path(cls.temporary.name)
        # A transport stub, not an implementation of the reference filter.
        stub = cls.root / 'stub.c'
        stub.write_text('void Syn_filt(short*a,short*x,short*y,short n,short*m,short u)'
                        '{int i;for(i=0;i<n;i++)y[i]=x[i];'
                        'if(u)for(i=0;i<10;i++)m[i]=y[n-10+i];}\n')
        cls.program = cls.root / 'calculator'
        subprocess.run(['gcc', '-std=gnu89', '-Wall', '-Wextra', '-o', str(cls.program),
                        str(Path(__file__).with_name('etsi_efr_filter_replay.c')), str(stub)],
                       check=True, capture_output=True)

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def run_harness(self, data):
        source, output = self.root / 'input.bin', self.root / 'output.bin'
        source.write_bytes(data)
        result = subprocess.run([str(self.program), str(source), str(output)], capture_output=True)
        return result, output.read_bytes()

    def test_little_endian_signed_words_and_multiple_calls(self):
        signal = tuple((0xffe0 + i) & 0xffff for i in range(40))
        arguments = struct.pack('<61H', *((0,) * 11 + signal + (0,) * 10))
        result, output = self.run_harness(arguments * 2)
        self.assertEqual(result.returncode, 0)
        self.assertIn(b'calls=2', result.stdout)
        self.assertEqual(output, struct.pack('<50H', *(signal + signal[-10:])) * 2)

    def test_empty_partial_words_and_partial_calls_fail(self):
        for data in (b'', b'\x01', b'\x00\x00' * 60):
            with self.subTest(size=len(data)):
                result, _ = self.run_harness(data)
                self.assertNotEqual(result.returncode, 0)
