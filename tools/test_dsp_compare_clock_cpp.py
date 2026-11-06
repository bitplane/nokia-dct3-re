"""Executable reference-phase arithmetic, independent of handset firmware."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


class CompareClockTest(unittest.TestCase):
    def test_phase_wrap_and_ceiling(self):
        compiler=shutil.which('c++')
        if not compiler: self.skipTest('C++ compiler unavailable')
        root=Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as directory:
            binary=Path(directory)/'compare-clock'
            subprocess.run([compiler,'-std=c++17','-Wall','-Wextra','-Werror',
                '-I',str(root/'driver'),str(root/'tests/unit/test_dsp_compare_clock.cpp'),
                '-o',str(binary)],check=True,capture_output=True,text=True)
            subprocess.run([str(binary)],check=True,capture_output=True,text=True)
