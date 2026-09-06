import pathlib
import tempfile
import unittest

from tools.radio_call_divert_trace_check import verify


GOOD = """
GSM service uplink sapi=0 pd=0b message=3b length=20 data=1b7b1c0da10b02010102010e30030401217f0100 t=1
gsm_ss: request=interrogate transaction=1b invoke=1 service=21 active=0 t=2
GSM service downlink kind=27 sapi=0 pd=0b message=2a length=17 t=3
radio_phase=release_channel_change t=4
"""


class RadioCallDivertTraceCheckTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.frames = pathlib.Path(self.directory.name)

    def tearDown(self):
        self.directory.cleanup()

    def test_complete_without_frame(self):
        verify(GOOD, self.frames, False)

    def test_rejects_wrong_service(self):
        with self.assertRaisesRegex(ValueError, "InterrogateSS"):
            verify(GOOD.replace("040121", "040143"), self.frames, False)

    def test_rejects_release_before_result(self):
        bad = GOOD.replace(
            "radio_phase=release_channel_change t=4\n", "").replace(
            "GSM service downlink", "radio_phase=release_channel_change t=2.5\nGSM service downlink")
        with self.assertRaisesRegex(ValueError, "RR release"):
            verify(bad, self.frames, False)


if __name__ == "__main__":
    unittest.main()
