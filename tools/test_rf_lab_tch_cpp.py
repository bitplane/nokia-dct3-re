"""Laboratory TCH/F loopback: schedule, FACCH, SACCH/TF and speech blocks."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


class RfLabTrafficChannelTest(unittest.TestCase):
    def test_downlink_bursts_decode_through_the_uplink_path(self):
        compiler=shutil.which('c++')
        if not compiler:self.skipTest('C++ compiler unavailable')
        root=Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as directory:
            binary=Path(directory)/'rf-lab-tch'
            subprocess.run([compiler,'-std=c++17','-Wall','-Wextra','-Werror','-I',str(root/'driver'),
                str(root/'tests/unit/test_rf_lab_tch.cpp'),str(root/'driver/gsm_tch_f_l1.cpp'),'-o',str(binary)],
                check=True,capture_output=True,text=True)
            result=subprocess.run([str(binary)],check=True,capture_output=True,text=True)
            self.assertIn('tch_lab facch=1 sacch=2',result.stdout)
            self.assertIn('tch_lab restore=exact',result.stdout)


if __name__=='__main__':unittest.main()
