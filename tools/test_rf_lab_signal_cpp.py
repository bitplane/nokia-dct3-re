"""Exercise ideal FCCH stimulus timing independently of MAME/firmware."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from tools.gsm_sch_signal import HYPERFRAME,sch_information_bits,sch_burst_bits,gmsk_burst_samples


class RfLabSignalTest(unittest.TestCase):
    def compile_fixture(self,directory):
        compiler=shutil.which('c++')
        if not compiler: self.skipTest('C++ compiler unavailable')
        root=Path(__file__).resolve().parents[1]
        binary=Path(directory)/'rf-lab-signal'
        subprocess.run([compiler,'-std=c++17','-Wall','-Wextra','-Werror',
                        '-I',str(root/'driver'),str(root/'tests/unit/test_rf_lab_signal.cpp'),
                        '-o',str(binary)],check=True,capture_output=True,text=True)
        return binary

    def test_fcch_burst_schedule(self):
        with tempfile.TemporaryDirectory() as directory:
            binary=self.compile_fixture(directory)
            result=subprocess.run([str(binary)],check=True,capture_output=True,text=True)
            self.assertIn('fcch_bursts=5 symbols=148 periodic=1 edges=1',result.stdout)

    def test_cpp_sch_iq_matches_independent_python_source(self):
        with tempfile.TemporaryDirectory() as directory:
            binary=self.compile_fixture(directory)
            for frame,bsic in [(1,0),(11,42),(1327,1),(HYPERFRAME-10,63)]:
                result=subprocess.run([str(binary),'sch',str(frame),str(bsic)],check=True,capture_output=True,text=True)
                actual=tuple(tuple(map(int,line.split())) for line in result.stdout.splitlines())
                expected=gmsk_burst_samples(sch_burst_bits(sch_information_bits(frame,bsic)))
                with self.subTest(frame=frame,bsic=bsic): self.assertEqual(actual,expected)


if __name__=='__main__': unittest.main()
