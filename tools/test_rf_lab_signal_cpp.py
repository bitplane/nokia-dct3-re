"""Exercise ideal FCCH stimulus timing independently of MAME/firmware."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from tools.gsm_sch_signal import HYPERFRAME,sch_information_bits,sch_burst_bits,gmsk_burst_samples
from tools.gsm_xcch_signal import control_bits,coded_control,check_vector,normal_control_bursts


class RfLabSignalTest(unittest.TestCase):
    def compile_fixture(self,directory):
        compiler=shutil.which('c++')
        if not compiler: self.skipTest('C++ compiler unavailable')
        root=Path(__file__).resolve().parents[1]
        binary=Path(directory)/'rf-lab-signal'
        subprocess.run([compiler,'-std=c++17','-Wall','-Wextra','-Werror',
                        '-I',str(root/'driver'),str(root/'tests/unit/test_rf_lab_signal.cpp'),
                        str(root/'driver/gsm_tch_f_l1.cpp'),
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

    def test_cpp_bcch_bits_and_iq_match_independent_reference(self):
        octets=bytes.fromhex('49061b000100f11000014000000000000000002b2b2b2b')
        bursts=[]
        with tempfile.TemporaryDirectory() as directory:
            binary=self.compile_fixture(directory)
            for burst in range(4):
                lines=subprocess.run([str(binary),'bcch',str(burst)],check=True,capture_output=True,text=True).stdout.splitlines()
                bursts.append(lines[0])
                self.assertEqual(tuple(map(int,lines[0])),normal_control_bursts(octets)[burst])
                actual=tuple(tuple(map(int,line.split())) for line in lines[1:])
                self.assertEqual(actual,gmsk_burst_samples(tuple(map(int,lines[0]))))
        check_vector(dict(octets=octets.hex(),tsc=2,normal_bursts=bursts,
                          information=''.join(map(str,control_bits(octets)[:184])),
                          coded=''.join(map(str,coded_control(octets)))))

    def test_cpp_assignment_bits_and_iq_match_independent_reference(self):
        octets=bytes.fromhex('2d063f002000230b09ae00002b2b2b2b2b2b2b2b2b2b2b')
        with tempfile.TemporaryDirectory() as directory:
            binary=self.compile_fixture(directory)
            for burst,bits in enumerate(normal_control_bursts(octets)):
                lines=subprocess.run([str(binary),'agch',str(burst)],check=True,
                                     capture_output=True,text=True).stdout.splitlines()
                self.assertEqual(tuple(map(int,lines[0])),bits)
                actual=tuple(tuple(map(int,line.split())) for line in lines[1:])
                self.assertEqual(actual,gmsk_burst_samples(bits))


if __name__=='__main__': unittest.main()
