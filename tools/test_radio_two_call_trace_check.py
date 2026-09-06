import pathlib
import tempfile
import unittest

from tools.radio_two_call_trace_check import verify


GOOD = """
GSM service uplink data=8347 t=1
waiting call SETUP transaction=13 malformed=0 duplicate=0 t=2
GSM service uplink data=93080802e091150101 t=3
GSM service uplink data=9341 t=4
call held count=1 transaction=83 leg=0 t=5
GSM service uplink data=9347 t=6
call held count=2 transaction=93 leg=1 t=7
call retrieved count=1 transaction=83 leg=0 t=8
state_roundtrip: result=pass t=9
GSM service uplink data=832502e090 t=10
GSM service uplink data=836a t=11
call retrieved count=2 transaction=93 leg=1 t=12
GSM service uplink data=936502e090 t=13
GSM service uplink data=932a t=14
radio_phase=release_channel_change t=15
"""


class TwoCallTraceCheckTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.frames = pathlib.Path(self.directory.name)

    def tearDown(self):
        self.directory.cleanup()

    def test_complete_lifecycle(self):
        self.assertEqual(13, verify(GOOD, self.frames, False)["events"])

    def test_rejects_transaction_alias(self):
        with self.assertRaisesRegex(ValueError, "second CONNECT"):
            verify(GOOD.replace("data=9347", "data=8347", 1), self.frames, False)

    def test_rejects_early_rr_release(self):
        bad = GOOD.replace(
            "GSM service uplink data=836a t=11\n",
            "GSM service uplink data=836a t=11\n"
            "radio_phase=release_channel_change t=11.5\n")
        with self.assertRaisesRegex(ValueError, "remained live"):
            verify(bad, self.frames, False)

    def test_requires_final_rr_release(self):
        with self.assertRaisesRegex(ValueError, "final call leg"):
            verify(GOOD.rsplit("radio_phase", 1)[0], self.frames, False)


if __name__ == "__main__":
    unittest.main()
