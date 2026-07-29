import unittest

from tools.radio_outgoing_call_outcome_trace_check import check


COMMON = """
GSM service establish sapi=0 pd=05 message=24 length=16 data=05247103331881080910101032547698
GSM service downlink kind=5 sapi=0 pd=05 message=21 length=2
GSM service uplink sapi=0 pd=03 message=05 length=15 data=03450401a05e0581551532f4150101
GSM outgoing request id=1 digits=5551234
GSM service downlink kind=10 sapi=0 pd=03 message=02 length=2
"""

BUSY = COMMON + """
GSM service downlink kind=13 sapi=0 pd=03 message=25 length=5
GSM service uplink sapi=0 pd=03 message=2d length=2 data=032d
GSM service downlink kind=20 sapi=0 pd=03 message=2a length=2
LAPDm service Channel Release acknowledged nr=2
PCH no-identity fill channel=60
"""

NO_ANSWER = COMMON + """
GSM service downlink kind=14 sapi=0 pd=06 message=2e length=8
GSM service uplink sapi=0 pd=06 message=29 length=3 data=062900
GSM service downlink kind=11 sapi=0 pd=03 message=01 length=2
GSM service uplink sapi=0 pd=03 message=25 length=5 data=032502e090
GSM service downlink kind=19 sapi=0 pd=03 message=2d length=2
GSM service uplink sapi=0 pd=03 message=2a length=2 data=036a
LAPDm service Channel Release acknowledged nr=3
PCH no-identity fill channel=60
"""

SERVICE_REJECT = """
GSM service establish sapi=0 pd=05 message=24 length=16 data=05247103331881080910101032547698
GSM service downlink kind=6 sapi=0 pd=05 message=22 length=3
LAPDm service Channel Release acknowledged nr=2
PCH no-identity fill channel=60
"""


class OutgoingCallOutcomeTraceTest(unittest.TestCase):
    def test_busy(self):
        check(BUSY, "busy")

    def test_no_answer(self):
        check(NO_ANSWER, "no-answer")

    def test_service_reject(self):
        check(SERVICE_REJECT, "service-reject")

    def test_busy_forbids_assignment(self):
        with self.assertRaisesRegex(ValueError, "traffic assignment"):
            check(BUSY + "GSM service downlink kind=14 sapi=0 pd=06 message=2e length=8", "busy")

    def test_no_answer_forbids_connect(self):
        with self.assertRaisesRegex(ValueError, "network Connect"):
            check(NO_ANSWER + "GSM service downlink kind=12 sapi=0 pd=03 message=07 length=2", "no-answer")

    def test_reject_forbids_request_state(self):
        with self.assertRaisesRegex(ValueError, "outgoing request"):
            check(SERVICE_REJECT + "GSM outgoing request id=1 digits=5551234", "service-reject")

    def test_save_state_marker_is_optional_but_enforceable(self):
        check(NO_ANSWER, "no-answer")
        with self.assertRaisesRegex(ValueError, "save-state"):
            check(NO_ANSWER, "no-answer", require_state_roundtrip=True)
        check(
            NO_ANSWER + "state_roundtrip: result=pass\n",
            "no-answer",
            require_state_roundtrip=True,
        )


if __name__ == "__main__":
    unittest.main()
