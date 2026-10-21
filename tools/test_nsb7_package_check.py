import hashlib
from pathlib import Path
import unittest

from tools.nsb7_package_check import inspect


class PackageTests(unittest.TestCase):
    def test_reject_unidentified_input(self):
        for source in (b'', b'MZ', b'PK\x03\x04'):
            with self.assertRaisesRegex(ValueError, 'acquired'):
                inspect(source)

    def test_acquired_package(self):
        path = Path(__file__).resolve().parents[1] / 'roms/archive-dct3-packages/nsb7_5.22.exe'
        if not path.exists():
            self.skipTest('acquired NSB-7 package unavailable')
        source = path.read_bytes()
        image, report = inspect(source)
        self.assertEqual(len(image), 0x1d0000)
        self.assertEqual(hashlib.sha1(image).hexdigest(), '4dacc50f57fe295168a53d3e5855a81acf35ab96')
        self.assertEqual(report['phone_streams'][1]['address'], '0x360000')
        self.assertEqual(image[0x160000:0x160004], b'PPM\x00')
        self.assertFalse(report['pmm_supplied'])
        self.assertFalse(report['boot_claim'])
        with self.assertRaises(ValueError):
            inspect(source[:-1])


if __name__ == '__main__':
    unittest.main()
