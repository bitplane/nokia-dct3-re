import unittest
import struct
import contextlib
import io
import json
from pathlib import Path
import tempfile
from unittest.mock import patch
from tools.mu4_recorder_media_inventory import inventory, fixture_volume, file_inventory, main, adts_header, adts_inventory, id3v23_inventory


class RecorderMediaInventoryTest(unittest.TestCase):
    def id3_fixture(self, length=None):
        def frame(name,payload):
            return name+len(payload).to_bytes(4,'big')+b'\0\0'+payload
        body = frame(b'TIT2',b'\0MAME')+frame(b'TPE1',b'\0TEST')
        if length is not None:
            body += frame(b'TLEN',b'\0'+length)
        body = body.ljust(100,b'\0')
        return b'ID3\x03\0\0\0\0\0\x64'+body+b'POCP'

    def test_tag_inventory_does_not_invent_missing_length(self):
        result = id3v23_inventory(self.id3_fixture())
        self.assertEqual([frame['id'] for frame in result['frames']],['TIT2','TPE1'])
        self.assertEqual(result['tag_end'],110)
        self.assertEqual(result['padding_bytes'],70)
        self.assertIsNone(result['declared_length_milliseconds'])
        self.assertEqual(id3v23_inventory(self.id3_fixture(b'1234'))['declared_length_milliseconds'],1234)

    def test_tag_inventory_rejects_unsupported_or_incomplete_fields(self):
        source = self.id3_fixture()
        for bad in (source[:20],source[:5]+b'\x80'+source[6:],
                    source[:6]+b'\x80'+source[7:],
                    source[:18]+b'\x01'+source[19:],
                    source[:14]+b'\x7f\xff\xff\xff'+source[18:],
                    source[:45]+b'X'+source[46:],self.id3_fixture(b'not-a-length')):
            with self.subTest(bad=bad[:30]), self.assertRaises(ValueError):
                id3v23_inventory(bad)
        with patch('sys.argv',['inventory','missing','missing','--id3-tags']), contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                main()

    def test_adts_cli_requires_file_mode_and_does_not_modify_inputs(self):
        with patch('sys.argv', ['inventory', 'missing', 'missing', '--adts-headers']), contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as error:
                main()
        self.assertEqual(error.exception.code, 2)
        frame = bytes.fromhex('fff950802e8600') + bytes(365)
        with tempfile.TemporaryDirectory() as directory:
            source, endpoint = Path(directory) / 'source', Path(directory) / 'endpoint'
            source.write_bytes(frame)
            endpoint.write_bytes(frame * 2)
            output = io.StringIO()
            with patch('sys.argv', ['inventory', str(source), str(endpoint), '--extracted-files', '--adts-headers']), contextlib.redirect_stdout(output):
                self.assertEqual(main(), 0)
            report = json.loads(output.getvalue())['adts_header_candidates']
            self.assertEqual(report['source']['first_candidate_chain'], [0])
            self.assertEqual(report['endpoint']['first_candidate_chain'], [0, 372])
            self.assertEqual(source.read_bytes(), frame)
            self.assertEqual(endpoint.read_bytes(), frame * 2)

    def test_adts_candidate_fields_are_not_payload_acceptance(self):
        frame = bytes.fromhex('fff950802e8600') + bytes(365)
        header = adts_header(frame, 0)
        self.assertEqual((header['frame_bytes'], header['mpeg_version'], header['profile_bits'],
                          header['sampling_index'], header['channel_configuration'], header['raw_data_blocks_minus_one']),
                         (372, 2, 1, 4, 2, 0))
        self.assertEqual(header['header_bytes'], 7)
        self.assertTrue(header['frame_within_file'])
        crc = bytes.fromhex('fff850802e8600') + bytes(365)
        self.assertEqual(adts_header(crc, 0)['header_bytes'], 9)
        mpeg4 = bytes.fromhex('fff150802e8600') + bytes(365)
        self.assertEqual(adts_header(mpeg4, 0)['mpeg_version'], 4)
        for invalid in (b'', frame[:6], bytes.fromhex('ffff50802e8600'), bytes.fromhex('fff95080000000')):
            self.assertIsNone(adts_header(invalid, 0))

    def test_adts_chain_walks_declared_lengths_and_keeps_divergent_candidates(self):
        frame = bytes.fromhex('fff950802e8600') + bytes(365)
        source = b'prefix' + frame * 2
        report = adts_inventory(source)
        self.assertEqual(report['first_candidate_chain'], [6, 378])
        self.assertEqual(report['chain_stop_reason'], 'exact_eof')
        divergent = bytes.fromhex('fff950442518c7') + bytes(365)
        report = adts_inventory(b'prefix' + frame * 2 + divergent + frame)
        self.assertEqual(report['candidate_count'], 4)
        self.assertEqual(report['first_candidate_chain'], [6, 378, 750])
        self.assertEqual(report['chain_stop'], 1046)
        self.assertEqual(report['chain_stop_reason'], 'missing_header_at_declared_end')
        self.assertEqual(report['candidates'][2]['channel_configuration'], 1)
        self.assertEqual(report['candidates'][2]['raw_data_blocks_minus_one'], 3)
        self.assertEqual(adts_inventory(frame[:20])['chain_stop_reason'], 'truncated_frame')
        self.assertEqual(adts_inventory(bytes(20))['chain_stop_reason'], 'no_candidate')

    def test_adts_payload_sync_candidate_does_not_replace_declared_chain(self):
        frame = bytes.fromhex('fff950802e8600') + bytes(365)
        first = bytearray(frame)
        first[20:27] = frame[:7]
        report = adts_inventory(bytes(first) + frame)
        self.assertEqual(report['candidate_count'], 3)
        self.assertEqual(report['first_candidate_chain'], [0, 372])
        self.assertEqual(report['chain_stop_reason'], 'exact_eof')

    def test_file_delta_separates_common_changes_from_added_tail(self):
        report = file_inventory(b'abcdef', b'abXYeZ\xff\xff')
        self.assertEqual(report['common_extent_bytes'], 6)
        self.assertEqual(report['changed_common_bytes'], 3)
        self.assertEqual(report['changed_common_ranges_end_exclusive'], [[2, 4], [5, 6]])
        self.assertEqual(report['added_bytes'], 2)
        self.assertEqual(report['added_byte_counts'], {'ff': 2})
        self.assertEqual(report['removed_bytes'], 0)
        self.assertEqual(report['removed_byte_counts'], {})

    def test_file_delta_shortening_and_empty_inputs(self):
        report = file_inventory(b'ab\x00\x00\xff', b'aB')
        self.assertEqual(report['changed_common_ranges_end_exclusive'], [[1, 2]])
        self.assertEqual(report['removed_bytes'], 3)
        self.assertEqual(report['removed_byte_counts'], {'00': 2, 'ff': 1})
        for source, endpoint in ((b'', b''), (b'abc', b'abc'), (b'', b'\xff')):
            report = file_inventory(source, endpoint)
            self.assertEqual(report['changed_common_bytes'], 0)
            self.assertEqual(report['changed_common_ranges_end_exclusive'], [])
            self.assertEqual(report['added_bytes'], len(endpoint) - len(source))

    def test_file_delta_cli_is_read_only_and_excludes_volume_export(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, endpoint = root / 'source', root / 'endpoint'
            source.write_bytes(b'abc')
            endpoint.write_bytes(b'abX\xff')
            arguments = ['inventory', str(source), str(endpoint), '--extracted-files']
            output = io.StringIO()
            with patch('sys.argv', arguments), contextlib.redirect_stdout(output):
                self.assertEqual(main(), 0)
            self.assertEqual(json.loads(output.getvalue())['added_byte_counts'], {'ff': 1})
            with patch('sys.argv', arguments + ['--export-fixture-volume', str(root / 'volume')]), contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                main()
            self.assertEqual(error.exception.code, 2)
            self.assertEqual(source.read_bytes(), b'abc')
            self.assertEqual(endpoint.read_bytes(), b'abX\xff')
            self.assertFalse((root / 'volume').exists())

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
