"""Exercise ideal FCCH stimulus timing independently of MAME/firmware."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class RfLabSignalTest(unittest.TestCase):
    def test_fcch_burst_schedule(self):
        compiler=shutil.which('c++')
        if not compiler: self.skipTest('C++ compiler unavailable')
        root=Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as directory:
            binary=Path(directory)/'rf-lab-signal'
            subprocess.run([compiler,'-std=c++17','-Wall','-Wextra','-Werror',
                            '-I',str(root/'driver'),str(root/'tests/unit/test_rf_lab_signal.cpp'),
                            '-o',str(binary)],check=True,capture_output=True,text=True)
            result=subprocess.run([str(binary)],check=True,capture_output=True,text=True)
            self.assertIn('fcch_bursts=5 symbols=148 periodic=1 edges=1',result.stdout)


if __name__=='__main__': unittest.main()
