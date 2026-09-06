import pathlib
import tempfile
import unittest

from tools.radio_call_divert_lifecycle_trace_check import verify


GOOD = """
radio_phase=release_channel_change
gsm_ss: request=register transaction=1b invoke=1 service=21 number_length=5 active=1
GSM service downlink kind=27 sapi=0 pd=0b message=2a length=33
radio_phase=release_channel_change
gsm_ss: request=interrogate transaction=1b invoke=2 service=21 active=1
GSM service downlink kind=27 sapi=0 pd=0b message=2a length=21
radio_phase=release_channel_change
gsm_ss: request=deactivate transaction=1b invoke=3 service=21 active=0
GSM service downlink kind=27 sapi=0 pd=0b message=2a length=33
radio_phase=release_channel_change
gsm_ss: request=interrogate transaction=1b invoke=4 service=21 active=0
GSM service downlink kind=27 sapi=0 pd=0b message=2a length=17
radio_phase=release_channel_change
state_roundtrip: result=pass
"""


class RadioCallDivertLifecycleTraceCheckTest(unittest.TestCase):
    def test_accepts_coherent_sequence_without_frames(self):
        with tempfile.TemporaryDirectory() as directory:
            result = verify(GOOD, pathlib.Path(directory), False)
        self.assertEqual(8, result["events"])
        self.assertEqual(5, result["releases"])

    def test_rejects_wrong_invoke_order(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "event 3"):
                verify(GOOD.replace("invoke=2", "invoke=7"),
                       pathlib.Path(directory), False)

    def test_requires_save_state_round_trip(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "save-state"):
                verify(GOOD.replace("state_roundtrip: result=pass\n", ""),
                       pathlib.Path(directory), False)


if __name__ == "__main__":
    unittest.main()
