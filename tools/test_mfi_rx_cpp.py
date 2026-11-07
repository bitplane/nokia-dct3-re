"""Execute clocked-word transport logic independently of handset firmware."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


class MfiRxTest(unittest.TestCase):
    def test_clock_blocks_provenance_and_snapshot(self):
        compiler=shutil.which('c++')
        if not compiler: self.skipTest('C++ compiler unavailable')
        root=Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as directory:
            binary=Path(directory)/'mfi-rx'
            subprocess.run([compiler,'-std=c++17','-Wall','-Wextra','-Werror',
                            '-I',str(root/'driver'),str(root/'tests/unit/test_mfi_rx.cpp'),
                            '-o',str(binary)],check=True,capture_output=True,text=True)
            result=subprocess.run([str(binary)],check=True,capture_output=True,text=True)
            self.assertIn('provenance=256',result.stdout)
            self.assertIn('staging96=3',result.stdout)


if __name__=='__main__': unittest.main()
