"""Keep isolated soft erasures separate from ADC/frontend and FIRE fixtures."""
from pathlib import Path
import subprocess
import sys
import unittest


class XcchErasureArgumentsTest(unittest.TestCase):
    def test_reject_mixed_boundaries_before_creating_a_fixture(self):
        script=Path(__file__).with_name('run_rom6_xcch_kernel.py')
        for flags in (['--erase-last-two','--erase-all'],
                      ['--erase-last-two','--frontend'],
                      ['--erase-all','--raw-adc'],
                      ['--erase-last-two','--fire-transaction']):
            with self.subTest(flags=flags):
                result=subprocess.run([sys.executable,str(script),'--entry-dir','.',*flags],
                                      capture_output=True,text=True)
                self.assertEqual(result.returncode,2)
                self.assertNotIn('ROM6 isolated xCCH evidence:',result.stdout)
