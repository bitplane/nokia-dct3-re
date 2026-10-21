"""Execute the production IDLE_RA timing arithmetic, including counter wrap."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class RadioTimingTests(unittest.TestCase):
    def test_production_timing_arithmetic(self):
        compiler = shutil.which('c++')
        if compiler is None:
            self.skipTest('C++ compiler unavailable')
        source = Path(__file__).with_suffix('.cpp')
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / 'radio-timing'
            subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror',
                            str(source), '-o', str(binary)], check=True, capture_output=True)
            subprocess.run([str(binary)], check=True, capture_output=True)


if __name__ == '__main__':
    unittest.main()
