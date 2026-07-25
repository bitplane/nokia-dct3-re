import unittest

from tools.dsp_upload_extract import encode_words, segmented_image, shared_snapshot


class DspUploadExtractTest(unittest.TestCase):
    def test_reassembles_contiguous_segmented_image(self):
        text = """
TX consume type=51 payload=6 words=4 data=220200010002 t=1
TX consume type=51 payload=4 words=3 data=22040003 t=2
"""
        self.assertEqual((0x2202, [1, 2, 3]), segmented_image(text))

    def test_rejects_gap(self):
        text = """
TX consume type=51 payload=4 words=3 data=22020001 t=1
TX consume type=51 payload=4 words=3 data=22040003 t=2
"""
        with self.assertRaisesRegex(ValueError, "non-contiguous"):
            segmented_image(text)

    def test_shared_snapshot_applies_only_writes_before_cutoff(self):
        text = """
dsp_shared_write: off=e00 old=0000 data=1234 pc=002b5bd0 t=0.1
dsp_shared_write: off=e04 old=0000 data=5678 pc=002b5bd0 t=0.2
dsp_shared_write: off=e00 old=1234 data=ffff pc=002b5bd0 t=0.3
"""
        words, observed = shared_snapshot(text, 0xE00, 0xE04, 0.25)
        self.assertEqual([0x1234, 0, 0x5678], words)
        self.assertEqual({0, 2}, observed)

    def test_word_encoding_is_explicit(self):
        self.assertEqual(b"\x12\x34", encode_words([0x1234], "big"))
        self.assertEqual(b"\x34\x12", encode_words([0x1234], "little"))


if __name__ == "__main__":
    unittest.main()
