import unittest
import struct
import contextlib
import io
from pathlib import Path
import tempfile
from unittest.mock import patch
from tools.mu4_recorder_media_inventory import inventory, fixture_volume, main


class RecorderMediaInventoryTest(unittest.TestCase):
    @staticmethod
    def volume_image():
        image = bytearray(528 * 8)
        image[528 + 510:528 + 512] = b'\x55\xaa'
        image[528 + 446 + 4] = 6
        struct.pack_into('<II', image, 528 + 446 + 8, 2, 3)
        boot = 3 * 528
        struct.pack_into('<H', image, boot + 11, 512)
        struct.pack_into('<H', image, boot + 19, 2)
        struct.pack_into('<I', image, boot + 28, 99)
        image[boot + 510:boot + 512] = b'\x55\xaa'
        image[4 * 528:4 * 528 + 512] = b'A' * 512
        for page in range(8):
            image[page * 528 + 512:(page + 1) * 528] = b'S' * 16
        return image

    def test_fixture_volume_uses_mbr_row_mapping_not_hidden_field(self):
        image = self.volume_image()
        original = bytes(image)
        metadata, volume = fixture_volume(image)
        self.assertEqual(volume, image[3 * 528:3 * 528 + 512] + b'A' * 512)
        self.assertEqual(metadata['first_physical_page'], 3)
        self.assertEqual(metadata['partition_lba'], 2)
        self.assertEqual(metadata['bpb_hidden_sectors'], 99)
        self.assertEqual(metadata['volume_bytes'], 1024)
        self.assertEqual(bytes(image), original)

    def test_fixture_volume_total32(self):
        image = self.volume_image()
        struct.pack_into('<H', image, 3 * 528 + 19, 0)
        struct.pack_into('<I', image, 3 * 528 + 32, 2)
        self.assertEqual(len(fixture_volume(image)[1]), 1024)

    def test_fixture_volume_rejects_signatures_and_sector_width(self):
        for offset in (528 + 510, 3 * 528 + 510, 3 * 528 + 12):
            image = self.volume_image()
            image[offset] = 0
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                fixture_volume(image)

    def test_fixture_volume_rejects_missing_multiple_or_other_partitions(self):
        for operation in ('missing', 'multiple', 'other'):
            image = self.volume_image()
            if operation == 'missing':
                image[528 + 446 + 4] = 0
            elif operation == 'multiple':
                image[528 + 462 + 4] = 6
            else:
                image[528 + 446 + 4] = 0x0b
            with self.subTest(operation=operation), self.assertRaises(ValueError):
                fixture_volume(image)

    def test_fixture_volume_rejects_extents(self):
        for lba, count, total in ((0, 3, 2), (8, 3, 2), (2, 0, 2),
                                  (2, 1, 2), (2, 3, 0), (2, 8, 6)):
            image = self.volume_image()
            struct.pack_into('<II', image, 528 + 446 + 8, lba, count)
            struct.pack_into('<H', image, 3 * 528 + 19, total)
            with self.subTest(lba=lba, count=count, total=total), self.assertRaises(ValueError):
                fixture_volume(image)

    def test_fixture_volume_rejects_partial_pages(self):
        for image in (b'', bytes(528), bytes(1055)):
            with self.subTest(size=len(image)), self.assertRaises(ValueError):
                fixture_volume(image)

    def test_export_cli_creates_new_file_without_overwriting_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, endpoint, volume = root / 'source', root / 'endpoint', root / 'volume'
            image = bytes(self.volume_image())
            source.write_bytes(image)
            endpoint.write_bytes(image)
            arguments = ['inventory', str(source), str(endpoint), '--export-fixture-volume', str(volume)]
            with patch('sys.argv', arguments), contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(main(), 0)
            expected = fixture_volume(image)[1]
            self.assertEqual(volume.read_bytes(), expected)
            with patch('sys.argv', arguments), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(main(), 1)
            self.assertEqual(volume.read_bytes(), expected)
            self.assertEqual(source.read_bytes(), image)
            self.assertEqual(endpoint.read_bytes(), image)

    def test_invalid_export_creates_no_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, endpoint, volume = root / 'source', root / 'endpoint', root / 'volume'
            source.write_bytes(bytes(528 * 8))
            endpoint.write_bytes(bytes(528 * 8))
            arguments = ['inventory', str(source), str(endpoint), '--export-fixture-volume', str(volume)]
            with patch('sys.argv', arguments), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(main(), 1)
            self.assertFalse(volume.exists())

    def test_unchanged(self):
        report = inventory(bytes(1056), bytes(1056))
        self.assertEqual(report['changed_pages'], [])
        self.assertEqual(report['source_sha256'], report['endpoint_sha256'])

    def test_data_and_spare_changes_are_separate(self):
        old = bytes(528 * 4)
        new = bytearray(old)
        new[528:531] = b'ID3'
        new[528 + 110:528 + 114] = b'POCP'
        new[2 * 528 + 512] = 1
        report = inventory(old, new)
        self.assertEqual(report['changed_pages'], [1, 2])
        self.assertEqual(report['changed_page_ranges_end_exclusive'], [[1, 3]])
        self.assertEqual(report['spare_changed_pages'], [2])
        self.assertEqual([(s['literal'], s['page'], s['data_offset'])
                          for s in report['raw_signatures']], [('ID3', 1, 0), ('POCP', 1, 110)])

    def test_signature_in_spare_is_not_payload(self):
        new = bytearray(528)
        new[512:515] = b'ID3'
        self.assertEqual(inventory(bytes(528), new)['raw_signatures'], [])

    def test_disjoint_runs_and_directory_literal(self):
        new = bytearray(528 * 3)
        new[:8] = b'REL_001 '
        new[1056] = 1
        report = inventory(bytes(len(new)), new)
        self.assertEqual(report['changed_page_ranges_end_exclusive'], [[0, 1], [2, 3]])
        self.assertEqual(report['raw_signatures'][0]['image_offset'], 0)

    def test_invalid_size_and_geometry(self):
        for old, new in ((b'', b''), (b'a', b'a'), (bytes(528), bytes(1056))):
            with self.assertRaises(ValueError):
                inventory(old, new)
        with self.assertRaises(ValueError):
            inventory(bytes(528), bytes(528), 528, 529)


if __name__ == '__main__':
    unittest.main()
