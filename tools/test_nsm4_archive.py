import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile

from tools import nsm4_archive as subject


class Nsm4ArchiveTest(unittest.TestCase):
    def fixture(self, names=('own',), address=0x200000):
        stream = io.BytesIO()
        raw = b'\x0b' + address.to_bytes(3, 'big') + b'\0\0\0\x02\0AB'
        with zipfile.ZipFile(stream, 'w') as archive:
            for name in names:
                archive.writestr(name, raw)
        return stream.getvalue(), {'own': (len(raw), 0x200000, 2)}

    def test_rejects_unpinned_archive(self):
        with self.assertRaisesRegex(ValueError, 'requires the acquired'):
            subject.decode_archive(b'not the acquired archive')

    def test_prefix_joins_only_own_contiguous_regions(self):
        with patch.object(subject, 'decode_archive', return_value=({},
                {subject.MCU: b'MCU', 'nsm4_5.18p': b'PPM'})):
            self.assertEqual(subject.image_prefix(b'', 'p'), b'MCUPPM')
        for pack in ('', 'a', 'pq'):
            with self.assertRaises(ValueError):
                subject.image_prefix(b'', pack)

    def test_decodes_without_padding_and_refuses_reuse(self):
        source, members = self.fixture()
        with patch.object(subject, 'ARCHIVE_SHA256', subject.sha256(source)), \
                patch.object(subject, 'MEMBERS', members), tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'new'
            report = subject.extract(source, output)
            self.assertEqual((output / 'own.bin').read_bytes(), b'AB')
            self.assertEqual(report, json.loads((output / 'manifest.json').read_text()))
            self.assertEqual(report['members'][0]['end_exclusive'], '0x200002')
            with self.assertRaises(FileExistsError):
                subject.extract(source, output)

    def test_rejects_wrong_members_sizes_and_addresses(self):
        for names, address, change_size in [(('other',), 0x200000, False),
                                             (('own',), 0x200002, False),
                                             (('own',), 0x200000, True)]:
            source, members = self.fixture(names, address)
            if change_size:
                members['own'] = (999, 0x200000, 2)
            with patch.object(subject, 'ARCHIVE_SHA256', subject.sha256(source)), \
                    patch.object(subject, 'MEMBERS', members):
                with self.assertRaises(ValueError):
                    subject.decode_archive(source)


if __name__ == '__main__':
    unittest.main()
