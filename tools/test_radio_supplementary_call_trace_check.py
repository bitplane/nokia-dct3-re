import pathlib
import tempfile
import unittest

from tools.radio_supplementary_call_trace_check import (
    UNHOLD_FRAME_SHA256,
    verify,
)


GOOD = """
dsp_hle: GSM service uplink data=83352c35 t=1
gsm_session: DTMF start digit=35 count=1 t=2
dsp_hle: GSM service uplink data=8371 t=3
gsm_session: DTMF stop digit=35 count=1 t=4
dsp_hle: GSM service uplink data=8318 t=5
gsm_session: call held count=1 t=6
state_roundtrip: result=pass t=7
dsp_hle: GSM service uplink data=835c t=8
gsm_session: call retrieved count=1 t=9
"""


class SupplementaryCallTraceCheckTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.frames = pathlib.Path(self.directory.name)

    def tearDown(self):
        self.directory.cleanup()

    def test_complete_protocol_without_frame_for_unit_fixture(self):
        result = verify(GOOD, self.frames, require_roundtrip=False)
        self.assertEqual(8, result["events"])

    def test_rejects_malformed_start_dtmf(self):
        with self.assertRaisesRegex(ValueError, "START DTMF"):
            verify(GOOD.replace("83352c35", "83352d35"), self.frames, False)

    def test_rejects_retrieve_before_hold(self):
        reordered = GOOD.replace(
            "dsp_hle: GSM service uplink data=8318 t=5\n"
            "gsm_session: call held count=1 t=6\n",
            "")
        with self.assertRaisesRegex(ValueError, "HOLD"):
            verify(reordered, self.frames, False)

    def test_requires_roundtrip_when_requested(self):
        with self.assertRaisesRegex(ValueError, "save-state"):
            verify(GOOD.replace("state_roundtrip: result=pass t=7\n", ""), self.frames)


if __name__ == "__main__":
    unittest.main()
