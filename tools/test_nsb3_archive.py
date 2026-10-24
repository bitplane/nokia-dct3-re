import hashlib
import io
import unittest
from unittest.mock import patch
import zipfile

from tools import nsb3_archive as subject


class Nsb3ArchiveTest(unittest.TestCase):
    def fixture(self, names=('own',)):
        raw = bytes.fromhex('0b2000000000000200') + b'AB'
        stream = io.BytesIO()
        with zipfile.ZipFile(stream, 'w') as archive:
            archive.writestr('nsb-3.ini', '[NSB-3_RESTOREFILES]\nEraseDefinedArea=Yes\nLowEraseLimit=200000\nHighEraseLimit=2FFFFF\n')
            for name in names:
                archive.writestr(name, raw)
        regions = {'own': (len(raw), 0x200000, 2, hashlib.sha256(b'AB').hexdigest())}
        return stream.getvalue(), regions

    def test_exact_region_without_padding(self):
        source, regions = self.fixture()
        with patch.object(subject, 'INSTALLER_SHA256', hashlib.sha256(source).hexdigest()), \
                patch.object(subject, 'REGIONS', regions), patch.object(subject, 'OTHER_MEMBERS', {'nsb-3.ini'}):
            report, decoded = subject.inspect(source)
            self.assertEqual(decoded, {'own': b'AB'})
            self.assertEqual(report['regions'][0]['end_exclusive'], '0x200002')

    def test_bad_pin_member_size_extent_and_hash(self):
        with self.assertRaises(ValueError):
            subject.inspect(b'not the installer')
        for names, field, value in [(('other',), None, None), (('own',), 0, 999),
                                    (('own',), 1, 0x200002), (('own',), 2, 3),
                                    (('own',), 3, 'wrong hash')]:
            source, regions = self.fixture(names)
            if field is not None:
                row = list(regions['own'])
                row[field] = value
                regions['own'] = tuple(row)
            with patch.object(subject, 'INSTALLER_SHA256', hashlib.sha256(source).hexdigest()), \
                    patch.object(subject, 'REGIONS', regions), patch.object(subject, 'OTHER_MEMBERS', {'nsb-3.ini'}):
                with self.assertRaises(ValueError):
                    subject.inspect(source)

    def test_installer_erased_gap_is_distinct_from_supplied_payload(self):
        regions = {'nsb3___6.130': b'M' * 0xd6d00, 'nsb3___6.13a': b'P' * 0x20000}
        image = subject.image_from_regions(regions, 'a')
        self.assertEqual(len(image), 0x100000)
        self.assertEqual(image[0xd6d00:0xe0000], b'\xff' * 0x9300)
        self.assertEqual(image[0xe0000:], regions['nsb3___6.13a'])
        with self.assertRaises(ValueError):
            subject.image_from_regions(regions, 'z')


if __name__ == '__main__':
    unittest.main()
