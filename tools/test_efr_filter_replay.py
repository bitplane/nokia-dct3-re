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
                        'if(u)for(i=0;i<10;i++)m[i]=y[n-10+i];}\n'
                        'void Pred_lt_6(short*e,short lag,short frac,short n)'
                        '{int i;for(i=0;i<n;i++)e[i]=e[-1];}\n'
                        'int L_mult(short a,short b){return 2*a*b;}\n'
                        'int L_mac(int v,short a,short b){return v+2*a*b;}\n'
                        'int L_shl(int v,short s){return v<<s;}\n'
                        'short fixture_round(int v)__asm__("round");\n'
                        'short fixture_round(int v){return(short)((v+32768)>>16);}\n'
                        'void agc2(short*x,short*y,short n)'
                        '{int i;for(i=0;i<n;i++)y[i]=x[i];}\n')
        cls.program = cls.root / 'calculator'
        subprocess.run(['gcc', '-std=gnu89', '-Wall', '-Wextra', '-o', str(cls.program),
                        str(Path(__file__).with_name('etsi_efr_argument_replay.c')), str(stub)],
                       check=True, capture_output=True)

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def run_harness(self, data, *, mode='filter'):
        source, output = self.root / 'input.bin', self.root / 'output.bin'
        source.write_bytes(data)
        result = subprocess.run([str(self.program), *(['--' + mode] if mode != 'filter' else []),
                                 str(source), str(output)], capture_output=True)
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

    def test_predictor_history_window_and_argument_domain(self):
        history = tuple(range(153)) + (0xfffe,)
        arguments = struct.pack('<156H', 45, 0xffff, *history)
        result, output = self.run_harness(arguments, mode='predictor')
        self.assertEqual(result.returncode, 0)
        self.assertEqual(output, struct.pack('<40H', *((0xfffe,) * 40)))
        for bad in (arguments[:-2], arguments[:1], b'',
                    struct.pack('<156H', 0, 0, *history),
                    struct.pack('<156H', 45, 4, *history)):
            with self.subTest(length=len(bad)):
                result, _ = self.run_harness(bad, mode='predictor')
                self.assertNotEqual(result.returncode, 0)

    def test_mixing_argument_order_and_partial_case(self):
        arguments = struct.pack('<82H', 1024, 4096, *((100,) * 40), *((200,) * 40))
        result, output = self.run_harness(arguments, mode='mix')
        self.assertEqual(result.returncode, 0)
        self.assertEqual(output, struct.pack('<40H', *((225,) * 40)))
        result, _ = self.run_harness(arguments[:-2], mode='mix')
        self.assertNotEqual(result.returncode, 0)

    def test_agc_argument_order_and_partial_case(self):
        signal = tuple(range(40))
        arguments = struct.pack('<80H', *(signal + (99,) * 40))
        result, output = self.run_harness(arguments, mode='agc')
        self.assertEqual(result.returncode, 0)
        self.assertEqual(output, struct.pack('<40H', *signal))
        result, _ = self.run_harness(arguments[:-2], mode='agc')
        self.assertNotEqual(result.returncode, 0)
