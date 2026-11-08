"""Finite external port framing; no physical FIFO or RF timing claim."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


class RfLabTransmitTest(unittest.TestCase):
    def test_framing_limits_replacement_cancel_and_snapshot(self):
        compiler=shutil.which('c++')
        if not compiler:self.skipTest('C++ compiler unavailable')
        root=Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as directory:
            binary=Path(directory)/'rf-lab-tx'
            subprocess.run([compiler,'-std=c++17','-Wall','-Wextra','-Werror',
                '-I',str(root/'driver'),str(root/'tests/unit/test_rf_lab_tx.cpp'),'-o',str(binary)],
                check=True,capture_output=True,text=True)
            subprocess.run([str(binary)],check=True,capture_output=True,text=True)


if __name__=='__main__':unittest.main()
