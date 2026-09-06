import pathlib
import tempfile
import unittest

from tools.radio_ussd_trace_check import verify


GOOD = """
GSM service uplink sapi=0 pd=0b message=3b length=27 data=1b7b1c14a11202010102013b300a04010f0405aa986c36027f0100 t=1
gsm_ss: request=ussd transaction=1b invoke=1 dcs=0f packed_length=5 t=1
GSM service downlink kind=27 sapi=0 pd=0b message=2a length=37 t=1
radio_phase=release_channel_change
"""


class RadioUssdTraceCheckTest(unittest.TestCase):
    def test_accepts_complete_protocol_without_frame_requirement(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertEqual(
                {"frames": 0}, verify(GOOD, pathlib.Path(directory), False))

    def test_rejects_missing_response(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "missing USSD ReturnResult"):
                verify(GOOD.replace("GSM service downlink", "missing"),
                       pathlib.Path(directory), False)

    def test_rejects_release_before_response(self):
        reordered = GOOD.replace(
            "GSM service downlink kind=27",
            "radio_phase=release_channel_change\nGSM service downlink kind=27")
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "missing RR release"):
                verify(reordered.rsplit("radio_phase=release_channel_change", 1)[0],
                       pathlib.Path(directory), False)


if __name__ == "__main__":
    unittest.main()
