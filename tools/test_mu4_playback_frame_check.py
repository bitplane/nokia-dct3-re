import struct
import unittest

from tools.mu4_playback_frame_check import compare_blocks, rows


class PlaybackFrameTests(unittest.TestCase):
    def test_snapshot_comparison_keeps_full_frame_and_channel_order(self):
        left,right = list(range(1024)),list(range(-1024,0))
        native = [(struct.pack('>1024h',*left),struct.pack('>1024h',*right))]
        reference = struct.pack('<2048h',*(value for pair in zip(left,right) for value in pair))
        result = compare_blocks(native,reference)
        self.assertEqual([row['mismatched_samples'] for row in result[0]['channels']],[0,0])
        left[448] += 100
        native[0] = struct.pack('>1024h',*left),native[0][1]
        result = compare_blocks(native,reference)
        self.assertEqual(result[0]['channels'][0]['first_mismatch'],{'sample':448,'native':548,'reference':448})
        self.assertEqual(result[0]['channels'][0]['mismatched_samples'],1)
        swapped = compare_blocks([native[0][::-1]],reference)
        self.assertEqual(swapped[0]['channels'][0]['mismatched_samples'],1024)

    def test_incomplete_or_empty_blocks_are_not_compared(self):
        for blocks,reference in (([],b''),([(bytes(2046),bytes(2048))],bytes(4096)),
                                 ([(bytes(2048),bytes(2048))],bytes(4092))):
            with self.assertRaises(ValueError):
                compare_blocks(blocks,reference)

    def test_observation_rows_do_not_drop_duplicate_or_malformed_fields(self):
        self.assertEqual(rows('[:] trace: ordinal=1 observer_writes=0','trace'),[{'ordinal':'1','observer_writes':'0'}])
        for text in ('trace: ordinal=1 ordinal=2 observer_writes=0',
                     'trace: ordinal=1 observer_writes=1','trace: missing observer_writes=0'):
            with self.assertRaises(ValueError):
                rows(text,'trace')


if __name__=='__main__':
    unittest.main()
