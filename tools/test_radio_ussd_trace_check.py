import pathlib
import tempfile
import unittest

from tools.radio_ussd_trace_check import verify


GOOD = """
GSM service uplink sapi=0 pd=0b message=3b length=27 data=1b7b1c14a11202010102013b300a04010f0405aa986c36027f0100 t=1
gsm_ss: request=ussd transaction=1b invoke=1 dcs=0f packed_length=5 outcome=0 t=1
GSM service downlink kind=27 sapi=0 pd=0b message=2a length=37 t=1
radio_phase=release_channel_change
"""
ERROR = GOOD.replace(
    "outcome=0", "outcome=1").replace(
    "length=37", "length=12") + """
RX enqueue type=80 payload=34 data=80120000132c000100000342319b2a1c08a306020101020122
"""
REJECT = GOOD.replace(
    "outcome=0", "outcome=2").replace(
    "length=37", "length=12") + """
RX enqueue type=80 payload=34 data=80120000132c000100000342319b2a1c08a406020101800100
"""
SILENCE = "\n".join(GOOD.replace("outcome=0", "outcome=3").splitlines()[:3])
CONTINUED_REJECTED = GOOD.replace(
    "outcome=0", "outcome=4").replace(
    "GSM service downlink kind=27 sapi=0 pd=0b message=2a length=37",
    "GSM service downlink kind=28 sapi=0 pd=0b message=3a length=29").replace(
    "radio_phase=release_channel_change", ""\
) + """
gsm_ss: continued handset_response message=2a length=6 data=1b2a0802e0e0 t=2
radio_phase=release_channel_change
"""


class RadioUssdTraceCheckTest(unittest.TestCase):
    def test_accepts_complete_protocol_without_frame_requirement(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertEqual(
                {"frames": 0},
                verify(GOOD, pathlib.Path(directory), require_frame=False))

    def test_accepts_error_and_reject_components(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory)
            self.assertEqual({"frames": 0}, verify(
                ERROR, path, "error", require_frame=False))
            self.assertEqual({"frames": 0}, verify(
                REJECT, path, "reject", require_frame=False))

    def test_accepts_silent_pending_dialogue(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertEqual({"frames": 0}, verify(
                SILENCE, pathlib.Path(directory), "silence",
                require_frame=False))

    def test_accepts_exact_continued_request_rejection(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertEqual({"frames": 0}, verify(
                CONTINUED_REJECTED, pathlib.Path(directory),
                "continued-rejected", require_frame=False))

    def test_accepts_silent_dialogue_state_roundtrip(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory)
            (path / "boot_summary.txt").write_text("state_roundtrip=pass\n")
            self.assertEqual({"frames": 0}, verify(
                SILENCE, path, "silence", require_frame=False,
                require_state_roundtrip=True))

    def test_rejects_missing_response(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "missing USSD success"):
                verify(GOOD.replace("GSM service downlink", "missing"),
                       pathlib.Path(directory), require_frame=False)

    def test_rejects_release_before_response(self):
        reordered = GOOD.replace(
            "GSM service downlink kind=27",
            "radio_phase=release_channel_change\nGSM service downlink kind=27")
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "missing RR release"):
                verify(reordered.rsplit("radio_phase=release_channel_change", 1)[0],
                       pathlib.Path(directory), require_frame=False)

    def test_rejects_response_in_silent_dialogue(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "unexpectedly sent"):
                verify(GOOD.replace("outcome=0", "outcome=3"),
                       pathlib.Path(directory), "silence", require_frame=False)


if __name__ == "__main__":
    unittest.main()
