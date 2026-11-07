"""Executable partial observed-word contract, not a physical HAGAR register spec."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


class HagarCommandTest(unittest.TestCase):
    def test_observed_words_and_fail_closed_transitions(self):
        compiler=shutil.which('c++')
        if not compiler: self.skipTest('C++ compiler unavailable')
        root=Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as directory:
            binary=Path(directory)/'hagar-command'
            subprocess.run([compiler,'-std=c++17','-Wall','-Wextra','-Werror',
                            '-I',str(root/'driver'),str(root/'tests/unit/test_hagar_command.cpp'),
                            '-o',str(binary)],check=True,capture_output=True,text=True)
            result=subprocess.run([str(binary)],check=True,capture_output=True,text=True)
            self.assertIn('keys=548',result.stdout)


if __name__=='__main__': unittest.main()
