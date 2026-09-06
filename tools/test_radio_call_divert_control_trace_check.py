import pathlib
import tempfile
import unittest

from tools.radio_call_divert_control_trace_check import verify


REGISTER = """
GSM service uplink sapi=0 pd=0b message=3b length=27 data=1b7b1c14a11202010102010a300a040121840581551532f47f0100 t=1
gsm_ss: request=register transaction=1b invoke=1 service=21 number_length=5 active=1 t=2
GSM service downlink kind=27 sapi=0 pd=0b message=2a length=14 t=3
radio_phase=release_channel_change t=4
"""


class RadioCallDivertControlTraceCheckTest(unittest.TestCase):
    def test_accepts_registration_without_frame_requirement(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertEqual({"frames": 0}, verify(
                "register", REGISTER, pathlib.Path(directory), False))

    def test_rejects_uncorrelated_operation(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "decoded register"):
                verify("register", REGISTER.replace("invoke=1", "invoke=2"),
                       pathlib.Path(directory), False)


if __name__ == "__main__":
    unittest.main()
