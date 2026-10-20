import gzip
import hashlib
from pathlib import Path
import unittest
import zlib

from tools.nhm9_package_check import inspect, members


class Nhm9PackageTest(unittest.TestCase):
    def test_adjacent_gzip_members(self):
        first, second = gzip.compress(b'first'), gzip.compress(b'second')
        self.assertEqual([(3, b'first'), (3 + len(first), b'second')],
                         list(members(b'abc' + first + second, 3)))

    def test_truncated_member(self):
        with self.assertRaises(ValueError):
            list(members(gzip.compress(b'data')[:-1], 0))

    def test_member_bound(self):
        with self.assertRaises(ValueError):
            list(members(gzip.compress(b'abcd'), 0, limit=3))

    def test_trailing_non_member(self):
        with self.assertRaises((ValueError, zlib.error)):
            list(members(gzip.compress(b'data') + b'invalid', 0))

    def test_unknown_installer(self):
        with self.assertRaisesRegex(ValueError, 'acquired NHM-9'):
            inspect(b'unknown')

    def test_acquired_package_when_available(self):
        path = Path(__file__).resolve().parents[1] / 'roms/archive-dct3-packages/nhm9_05.30_install.EXE'
        if not path.exists():
            self.skipTest('acquired NHM-9 installer absent')
        images, report = inspect(path.read_bytes())
        self.assertEqual(29, report['gzip_members'])
        self.assertEqual(7, len(report['phone_streams']))
        self.assertFalse(report['pmm_supplied'])
        self.assertFalse(report['boot_claim'])
        self.assertEqual(0x270000, len(images['mcu']))
        for pack in 'ABCDEF':
            self.assertEqual(0xc0000, len(images[pack]))
            self.assertEqual(ord(pack), images[pack][40])
        normalized = images['mcu'] + images['A']
        self.assertEqual(0x330000, len(normalized))
        self.assertEqual('3d0dce9c098bdd619c6f3684f739796f64b66169', hashlib.sha1(normalized).hexdigest())


if __name__ == '__main__':
    unittest.main()
