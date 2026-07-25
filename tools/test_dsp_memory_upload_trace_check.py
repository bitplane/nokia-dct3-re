import unittest

from tools.dsp_memory_upload_trace_check import verify


GOOD = """
dspif_transport: TX consume type=51 payload=6 words=4 consumer=01 data=220200010002 t=1
dsp_hle: data memory upload first=2202 words=2 last=2203 t=1
dspif_transport: TX consume type=51 payload=4 words=3 consumer=02 data=22040003 t=2
dsp_hle: data memory upload first=2204 words=1 last=2204 t=2
"""


class DspMemoryUploadTraceCheckTest(unittest.TestCase):
    def test_complete_upload(self):
        self.assertEqual(
            {"first": 0x2202, "last": 0x2204, "words": 3, "fragments": 2},
            verify(GOOD),
        )

    def test_rejects_dropped_application(self):
        with self.assertRaisesRegex(ValueError, "differ"):
            verify(GOOD.replace(
                "dsp_hle: data memory upload first=2204 words=1 last=2204 t=2", ""))

    def test_rejects_address_gap(self):
        with self.assertRaisesRegex(ValueError, "contiguous"):
            verify(GOOD.replace("data=22040003", "data=22050003").replace(
                "first=2204 words=1 last=2204", "first=2205 words=1 last=2205"))


if __name__ == "__main__":
    unittest.main()
