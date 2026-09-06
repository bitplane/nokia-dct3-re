import pathlib
import tempfile
import unittest

from tools.radio_second_outgoing_call_trace_check import verify


GOOD = """
GSM service uplink data=8347 t=1
gsm_session: call held count=1 transaction=83 leg=0 t=2
gsm_session: second outgoing CM service request transaction=05 live_legs=1 t=3
gsm_session: second outgoing SETUP transaction=13 leg=1 digits=8 t=4
GSM service uplink data=134f t=5
"""


class SecondOutgoingCallTraceCheckTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.frames = pathlib.Path(self.directory.name)

    def tearDown(self):
        self.directory.cleanup()

    def test_complete_protocol_without_frames(self):
        self.assertEqual(5, verify(GOOD, self.frames, False)["events"])

    def test_rejects_second_setup_before_nested_service(self):
        lines = GOOD.splitlines()
        reordered = "\n".join(lines[:2] + [lines[4], lines[2], lines[3], lines[5:][0]])
        with self.assertRaisesRegex(ValueError, "second SETUP"):
            verify(reordered, self.frames, False)

    def test_rejects_transaction_alias(self):
        with self.assertRaisesRegex(ValueError, "second SETUP"):
            verify(GOOD.replace("transaction=13", "transaction=83"), self.frames, False)

    def test_rejects_call_related_facility(self):
        with self.assertRaisesRegex(ValueError, "FACILITY"):
            verify(GOOD + "GSM service uplink data=133a08a10602010102017c t=6\n",
                   self.frames, False)


if __name__ == "__main__":
    unittest.main()
