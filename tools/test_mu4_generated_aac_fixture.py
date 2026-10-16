import json
import math
from pathlib import Path
import struct
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from tools.mu4_generated_aac_fixture import (
    embed_fixture_volume, fixture_authoring_policy, generate, pcm_tone_report,
    run, titled_track, validate_generated_headers,
)
from tools.mu4_recorder_media_inventory import adts_inventory, fixture_volume


class GeneratedAACFixtureTest(unittest.TestCase):
    @staticmethod
    def image():
        image = bytearray(528 * 8)
        mbr = 528
        image[mbr + 510:mbr + 512] = b'\x55\xaa'
        image[mbr + 450] = 6
        struct.pack_into('<II', image, mbr + 454, 2, 4)
        boot = 3 * 528
        struct.pack_into('<H', image, boot + 11, 512)
        image[boot + 13] = 1
        struct.pack_into('<H', image, boot + 14, 1)
        image[boot + 16] = 1
        struct.pack_into('<H', image, boot + 17, 16)
        struct.pack_into('<H', image, boot + 19, 4)
        image[boot + 21] = 0xf8
        struct.pack_into('<H', image, boot + 22, 1)
        image[boot + 510:boot + 512] = b'\x55\xaa'
        image[4 * 528:4 * 528 + 3] = b'\xf8\xff\xff'
        image[6 * 528:6 * 528 + 512] = b'D' * 512
        for page in range(8):
            image[page * 528 + 512:(page + 1) * 528] = b'S' * 16
        return bytes(image)

    def test_title_and_artist_are_external_id3_data(self):
        track = titled_track(b'payload')
        self.assertEqual(track[:10], b'ID3\x03\0\0\0\0\0\x64')
        self.assertEqual(track[110:], b'payload')
        self.assertIn(b'TIT2\0\0\0\5\0\0\0MAME', track)
        self.assertIn(b'TPE1\0\0\0\5\0\0\0TEST', track)

    def test_embedding_preserves_spares_mbr_and_unrelated_pages(self):
        original = self.image()
        _, volume = fixture_volume(original)
        edited = volume[:-512] + b'Y' * 512
        output, report = embed_fixture_volume(original, edited)
        expected = bytearray(original)
        expected[6 * 528:6 * 528 + 512] = b'Y' * 512
        self.assertEqual(output, bytes(expected))
        self.assertEqual(report['changed_data_pages'], 1)
        self.assertTrue(report['spare_bytes_preserved'])
        for page in range(8):
            self.assertEqual(output[page * 528 + 512:(page + 1) * 528], b'S' * 16)
        for bad in (volume[:-1], b'X' + volume[1:]):
            with self.assertRaises(ValueError):
                embed_fixture_volume(original, bad)

    def test_compatibility_override_is_limited_to_known_fat12_mismatch(self):
        _, volume = fixture_volume(self.image())
        self.assertFalse(fixture_authoring_policy(volume)['skip_check'])
        legacy = bytearray(volume)
        legacy[512:515] = b'\xff\xff\xff'
        self.assertTrue(fixture_authoring_policy(legacy)['skip_check'])
        for operation in ('other_prefix', 'other_media', 'fat16', 'sector_width'):
            changed = bytearray(legacy)
            if operation == 'other_prefix':
                changed[512] = 0xf7
            elif operation == 'other_media':
                changed[21] = 0xf0
            elif operation == 'fat16':
                struct.pack_into('<H', changed, 19, 50000)
            else:
                struct.pack_into('<H', changed, 11, 1024)
            with self.subTest(operation=operation), self.assertRaises(ValueError):
                fixture_authoring_policy(changed)

    def test_header_chain_allows_payload_candidates_but_not_wrong_configuration(self):
        frame = bytes.fromhex('fff950802e8600') + bytes(365)
        first = bytearray(frame)
        first[20:27] = frame[:7]
        validate_generated_headers(adts_inventory(bytes(first) + frame))
        for bad in (b'prefix' + frame, frame[:20], frame.replace(b'\xff\xf9', b'\xff\xf1', 1)):
            with self.assertRaises(ValueError):
                validate_generated_headers(adts_inventory(bad))

    def test_host_tone_check_requires_nonzero_distinct_channels(self):
        pcm = b''.join(struct.pack('<hh', round(3000 * math.sin(2 * math.pi * 440 * i / 44100)),
                                   round(1500 * math.sin(2 * math.pi * 660 * i / 44100)))
                       for i in range(44100 * 3))
        report = pcm_tone_report(pcm)
        self.assertEqual(report['stereo_frames'], 132300)
        self.assertAlmostEqual(report['tones'][0]['amplitude_s16'], 3000, delta=1)
        self.assertAlmostEqual(report['tones'][1]['amplitude_s16'], 1500, delta=1)
        for bad in (pcm[:-1], pcm[:100], bytes(len(pcm))):
            with self.assertRaises(ValueError):
                pcm_tone_report(bad)

    def test_existing_destination_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, output = root / 'source.nand', root / 'output'
            source.write_bytes(self.image())
            output.mkdir()
            marker = output / 'marker'
            marker.write_bytes(b'keep')
            with patch('tools.mu4_generated_aac_fixture.run') as runner, self.assertRaises(FileExistsError):
                generate(source, output)
            runner.assert_not_called()
            self.assertEqual(marker.read_bytes(), b'keep')
            self.assertEqual(source.read_bytes(), self.image())

    def test_independent_readback_mismatch_prevents_nand_publication(self):
        frame = bytes.fromhex('fff950802e8600') + bytes(365)
        def command(argv, environment):
            if argv[0] == 'ffmpeg':
                Path(argv[-1]).write_bytes(frame if 'lavfi' in argv else b'pcm')
                return SimpleNamespace(stdout=b'', stderr=b'')
            if argv[0] == 'ffprobe':
                return SimpleNamespace(stdout=json.dumps({'streams': [dict(codec_name='aac', profile='LC', sample_rate='44100', channels=2)]}).encode())
            if argv[0] == 'mcopy':
                self.assertEqual(argv[-1], '::TEST.REL')
                self.assertEqual(environment['SOURCE_DATE_EPOCH'], '946684800')
                return SimpleNamespace(stdout=b'')
            self.assertEqual(argv[0], '7z')
            self.assertEqual(argv[-1], 'TEST*')
            return SimpleNamespace(stdout=b'wrong file')
        with tempfile.TemporaryDirectory() as directory:
            source, output = Path(directory) / 'source.nand', Path(directory) / 'output'
            source.write_bytes(self.image())
            with patch('tools.mu4_generated_aac_fixture.run', side_effect=command), \
                    patch('tools.mu4_generated_aac_fixture.pcm_tone_report', return_value={}):
                with self.assertRaisesRegex(ValueError, 'independent FAT readback'):
                    generate(source, output)
            self.assertFalse((output / 'control.nand').exists())
            self.assertEqual(source.read_bytes(), self.image())

    def test_external_command_failure_is_not_ignored(self):
        failed = SimpleNamespace(returncode=1, stderr=b'failed', stdout=b'')
        with patch('subprocess.run', return_value=failed), self.assertRaisesRegex(ValueError, 'failed'):
            run(['writer'], {})


if __name__ == '__main__':
    unittest.main()
