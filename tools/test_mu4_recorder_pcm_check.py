import json
import hashlib
from pathlib import Path
import struct
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from tools.mu4_recorder_pcm_check import validate_capture, correlation_matrix, decode_projection, qualify_projection
from tools.mu4_transform_check import fnv


class RecorderPcmTests(unittest.TestCase):
    def test_capture_rejects_altered_partial_and_unordered_words(self):
        raw = struct.pack('<4096h', *range(-2048, 2048))
        text = (f'mu4_recorder_input_pcm: file=mu4_recorder_input.pcm words=4096 fnv1a={fnv(raw):08x} '
                'bad_order=0 dropped=0 cap=1048576 channels=2 format=s16le_interleaved '
                'source=staged_operand_read save_restore=unsupported observer_writes=0 audio_acceptance=0')
        consumer = dict(frames=2, origins=4096, known_reads=4096, read_epochs=4096)
        self.assertEqual(validate_capture(text, raw, consumer)['stereo_frames'], 2048)
        for before, after in [('words=4096', 'words=4095'), ('bad_order=0', 'bad_order=1'),
                              ('dropped=0', 'dropped=1'), ('channels=2', 'channels=1'),
                              ('audio_acceptance=0', 'audio_acceptance=1')]:
            with self.subTest(before=before), self.assertRaises(ValueError):
                validate_capture(text.replace(before, after), raw, consumer)
        for invalid in (raw[:-2], b'\x01'+raw[1:]):
            with self.assertRaises(ValueError):
                validate_capture(text, invalid, consumer)
        with self.assertRaises(ValueError):
            validate_capture(text+'\n'+text, raw, consumer)

    def test_bounded_correlation_recovers_delay_and_keeps_channel_matrix(self):
        import numpy as np
        x = np.random.default_rng(123).integers(-12000, 12000, (16384, 2), dtype=np.int16)
        y = np.zeros_like(x)
        y[17:, 0] = x[:-17, 0]//2
        y[29:, 1] = x[:-29, 1]//2
        result = correlation_matrix(x.astype('<i2').tobytes(), y.astype('<i2').tobytes(), margin=64, max_lag=32)
        diagonal = [row for row in result['matrix'] if row['input_channel'] == row['output_channel']]
        self.assertEqual([row['fitted_delay_samples'] for row in diagonal], [17, 29])
        for row in diagonal:
            self.assertGreater(row['correlation'], .999)
            self.assertAlmostEqual(row['affine_gain'], .5, places=4)
        self.assertEqual(len(result['matrix']), 4)
        for row in result['matrix']:
            if row['input_channel'] != row['output_channel']:
                self.assertLess(abs(row['correlation']), .05)
        self.assertFalse(result['alignment_validated'])
        self.assertFalse(result['codec_conformance_validated'])

    def test_periodic_input_does_not_claim_unique_latency(self):
        import numpy as np
        x = np.where(np.arange(4096)%16 < 8, 10000, -10000).astype('<i2')
        raw = np.column_stack((x, x)).astype('<i2').tobytes()
        result = correlation_matrix(raw, raw, margin=64, max_lag=32)
        self.assertGreater(result['matrix'][0]['near_peak_offsets'], 1)
        self.assertFalse(result['alignment_validated'])

    def test_silence_is_unassessable_not_a_content_pass(self):
        result = correlation_matrix(bytes(1024*4), bytes(1024*4), margin=64, max_lag=32)
        self.assertTrue(all(not row['assessable'] for row in result['matrix']))
        with self.assertRaises(ValueError):
            correlation_matrix(bytes(8), bytes(8))

    def test_projected_decode_requires_complete_packets_and_clean_coverage(self):
        # One AAC-LC stereo ADTS packet; payload validity belongs to the mocked decoder.
        packet = bytes.fromhex('fff15080011ffc00')
        streams = json.dumps(dict(streams=[dict(codec_name='aac', profile='LC', channels=2, sample_rate='44100')])).encode()
        responses = [SimpleNamespace(stdout=streams, stderr=b''),
                     SimpleNamespace(stdout=bytes(1024*4), stderr=b''),
                     SimpleNamespace(stdout=b'ffmpeg test\n', stderr=b'')]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'projection.aac'
            path.write_bytes(packet)
            with patch('tools.mu4_recorder_pcm_check.subprocess.run', side_effect=responses):
                decoded, report = decode_projection(path)
            self.assertEqual(len(decoded), 4096)
            self.assertFalse(report['host_projection_derivation_validated'])
            for raw in (packet[:-1], packet+b'bad', packet*513):
                path.write_bytes(raw)
                with self.assertRaises(ValueError):
                    decode_projection(path)
            path.write_bytes(packet)
            with patch('tools.mu4_recorder_pcm_check.subprocess.run', side_effect=[responses[0], SimpleNamespace(stdout=b'', stderr=b'bad frame')]):
                with self.assertRaises(ValueError):
                    decode_projection(path)

    def test_projection_qualification_rejects_wrong_volume_and_payload(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            endpoint, volume, trace, projected = (root/name for name in ('nand', 'volume', 'trace', 'aac'))
            endpoint.write_bytes(b'endpoint'); volume.write_bytes(b'volume')
            trace.write_text('trace'); projected.write_bytes(b'aac')
            metadata = dict(volume_sha256=hashlib.sha256(b'volume').hexdigest())
            proof = dict(native_beyond_prefix_validated=False, captured_prefix_bytes=3072)
            with patch('tools.mu4_recorder_pcm_check.recording_contract'), \
                    patch('tools.mu4_recorder_pcm_check.fixture_volume', return_value=(metadata, b'volume')), \
                    patch('tools.mu4_recorder_pcm_check.subprocess.run', return_value=SimpleNamespace(stdout=b'stored')), \
                    patch('tools.mu4_recorder_pcm_check.project_complete_windows', return_value=(proof, b'aac')):
                result = qualify_projection(endpoint, volume, trace, projected, b'source')
                self.assertTrue(result['host_projection_derivation_validated'])
                self.assertFalse(result['native_projection_beyond_prefix_validated'])
                projected.write_bytes(b'wrong')
                with self.assertRaises(ValueError):
                    qualify_projection(endpoint, volume, trace, projected, b'source')
                projected.write_bytes(b'aac'); volume.write_bytes(b'wrong')
                with self.assertRaises(ValueError):
                    qualify_projection(endpoint, volume, trace, projected, b'source')


if __name__ == '__main__':
    unittest.main()
