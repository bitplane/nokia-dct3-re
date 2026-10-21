from pathlib import Path
import unittest
from tools.nsb7_native_boundary_check import check


class NativeBoundaryTests(unittest.TestCase):
    def test_own_native_boundary_and_negative_controls(self):
        path = Path(__file__).resolve().parents[1] / 'roms/noki8290/8290f522a.fls'
        if not path.exists():
            self.skipTest('own normalized NSB-7 input absent')
        image = path.read_bytes()
        release = 'release entry=0f00 words=223 prom_input=0006 clock=13000000 stage=verifier'
        trace = '\n'.join([release, release,
            'publication word0=0000 word1=0006 word2=0006 word3=0006 pc=0f64',
            'release entry=0f00 words=126 prom_input=0006 clock=13000000 stage=loader',
            'loader2_verified words=623 entry=0a00'])
        output = 'Staged DSP escaped uploaded program: pc=2c75'
        check(image, output, trace, 1)
        for bad in (trace.replace('words=623', 'words=638'), trace + '\nruntime_hle_handoff',
                    trace.replace('word3=0006', 'word3=ffff'), '\n'.join(trace.splitlines()[::-1])):
            with self.assertRaises(ValueError):
                check(image, output, bad, 1)
        for code in (0, -11):
            with self.assertRaises(ValueError):
                check(image, output, trace, code)


if __name__ == '__main__':
    unittest.main()
