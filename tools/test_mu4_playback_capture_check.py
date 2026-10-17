import struct
import tempfile
import unittest
from array import array
from pathlib import Path
from unittest.mock import patch

from tools.mu4_playback_capture_check import (
    FILES, compare_streams, parse_rows, validate_stream, verify,
)


class PlaybackCaptureTests(unittest.TestCase):
    @staticmethod
    def stream(words):
        payload = struct.pack('<'+'H'*len(words),*words)
        fingerprint = 2166136261
        for byte in struct.pack('>'+'H'*len(words),*words):
            fingerprint = ((fingerprint ^ byte)*16777619) & 0xffffffff
        return payload, {'words':len(words),'bytes':len(payload),'hash':f'{fingerprint:08x}'}

    def fixture(self):
        payload,row = self.stream([0,1,0xffff,0x1234])
        text = '\n'.join(
            f'[:] mu4_playback_word_capture: boundary={name} words={row["words"]} bytes={row["bytes"]} '
            f'hash={row["hash"]} encoding=le16 file={filename} save_restore=unsupported observer_writes=0 audio_acceptance=0'
            for name,filename in FILES.items())
        return payload,text

    def test_exact_word_encoding_and_observed_hash(self):
        payload,row = self.stream([0,0x1234,0x8000,0xffff])
        words,report = validate_stream(row,payload)
        self.assertEqual(list(words),[0,0x1234,0x8000,0xffff])
        self.assertEqual(report['nonzero'],3)
        self.assertEqual(report['observed_hash'],row['hash'])

    def test_incomplete_changed_or_byte_swapped_stream_is_rejected(self):
        payload,row = self.stream([0,0x1234,0x8000,0xffff])
        for bad in (payload[:-1],payload[:-2],payload+b'\0\0',
                    payload[:2]+b'\0\0'+payload[4:],
                    b''.join(payload[i:i+2][::-1] for i in range(0,len(payload),2))):
            with self.subTest(bad=bad),self.assertRaises(ValueError):
                validate_stream(row,bad)
        for key in ('words','bytes'):
            changed = dict(row)
            changed[key] += 1
            with self.assertRaises(ValueError):
                validate_stream(changed,payload)

    def test_boundary_rows_are_complete_unique_and_fixed(self):
        _,text = self.fixture()
        self.assertEqual(set(parse_rows(text)),set(FILES))
        for bad in ('\n'.join(text.splitlines()[1:]),text+'\n'+text.splitlines()[0],
                    text.replace('boundary=dma_source','boundary=cpu_read'),
                    text.replace('mu4_playback_dma.le16','wrong.le16'),
                    text.replace('observer_writes=0','observer_writes=1'),
                    text.replace('save_restore=unsupported','save_restore=supported'),
                    text.replace('audio_acceptance=0','audio_acceptance=1'),
                    text+' extra'):
            with self.subTest(bad=bad),self.assertRaises(ValueError):
                parse_rows(bad)

    def test_first_nonzero_alignment_checks_all_overlap_and_extras(self):
        source = array('H',[0,0,1,2,3,4,5,6])
        sink = array('H',[0,0,0,0,1,2,3,4,5,6,0])
        result = compare_streams(source,sink,anchor=4)
        self.assertTrue(result['alignment_established'])
        self.assertEqual(result['sink_offset_words'],2)
        self.assertEqual(result['compared_words'],8)
        self.assertEqual(result['mismatched_words'],0)
        self.assertEqual(result['exact_prefix_words'],8)
        self.assertEqual(result['sink_suffix_words'],1)
        self.assertEqual(result['sink_extra_nonzero'],0)
        self.assertFalse(result['transport_pairing_validated'])

    def test_tail_corruption_is_not_hidden_by_a_matching_anchor(self):
        source = array('H',[0,1,2,3,4,5,6,7])
        sink = array('H',[0,0,1,2,3,4,5,99,7])
        result = compare_streams(source,sink,anchor=4)
        self.assertEqual(result['mismatched_words'],1)
        self.assertEqual(result['exact_prefix_words'],6)
        self.assertEqual(result['first_mismatches'][0]['source_index'],6)
        self.assertFalse(result['transport_pairing_validated'])

    def test_alignment_does_not_search_past_an_initial_disagreement(self):
        source = array('H',[0,1,2,3,4,5])
        sink = array('H',[0,9,9,9,1,2,3,4,5])
        self.assertFalse(compare_streams(source,sink,anchor=4)['alignment_established'])
        for words in ([],[0]*8,[0,1]):
            self.assertFalse(compare_streams(array('H',words),source,anchor=4)['alignment_established'])
        with self.assertRaises(ValueError):
            compare_streams(source,sink,anchor=0)

    def test_wrapper_requires_existing_playback_validation_and_boundary_counters(self):
        payload,text = self.fixture()
        observation = {'tx_words':4,'din_words':4,'dma_reads':4,
                       'tx_nonzero':3,'din_nonzero':3,'dma_nonzero':3}
        observation.update(tx_hash=parse_rows(text)['mcbsp_tx']['hash'],
                           din_hash=parse_rows(text)['codec_din']['hash'])
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for filename in FILES.values():
                (root/filename).write_bytes(payload)
            with patch('tools.mu4_playback_capture_check.verify_playback_dma',return_value=observation) as checked:
                report = verify(text,root)
                checked.assert_called_once_with(text)
                self.assertTrue(report['capture_integrity_validated'])
                self.assertFalse(report['native_audio_validated'])
            with patch('tools.mu4_playback_capture_check.verify_playback_dma',side_effect=ValueError('illegal')):
                with self.assertRaises(ValueError):
                    verify(text,root)
            for key in observation:
                changed = dict(observation)
                changed[key] = f'{int(changed[key],16)^1:08x}' if key.endswith('_hash') else changed[key]+1
                with patch('tools.mu4_playback_capture_check.verify_playback_dma',return_value=changed):
                    with self.subTest(key=key),self.assertRaises(ValueError):
                        verify(text,root)


if __name__ == '__main__':
    unittest.main()
