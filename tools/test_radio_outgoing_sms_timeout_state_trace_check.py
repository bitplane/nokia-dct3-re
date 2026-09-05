import unittest

from tools.test_radio_outgoing_sms_timeout_trace_check import GOOD
from tools.radio_outgoing_sms_timeout_state_trace_check import verify


STATE = (
    "state_roundtrip: result=pass timer_delta=0000 mode=0004 "
    "requested_at=40.000000 t=40.000000\n")


class OutgoingSmsTimeoutStateTraceCheckTest(unittest.TestCase):
    def test_round_trip_between_submit_and_retry(self):
        verify(GOOD + STATE)

    def test_rejects_failed_round_trip(self):
        with self.assertRaisesRegex(ValueError, "did not pass"):
            verify(GOOD + STATE.replace("result=pass", "result=fail"))

    def test_rejects_round_trip_after_retry(self):
        with self.assertRaisesRegex(ValueError, "did not occur between"):
            verify(GOOD + STATE.replace("40.000000", "60.000000"))


if __name__ == "__main__":
    unittest.main()
