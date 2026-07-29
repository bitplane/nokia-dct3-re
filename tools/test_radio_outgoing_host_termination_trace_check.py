import unittest

from tools.radio_outgoing_host_termination_trace_check import check


TRACE = """
GSM outgoing request id=1 digits=5551234
gsm_call_adapter: decision id=1 outcome=0 result=accepted
gsm_call_adapter: termination id=2 cause=16 result=rejected
gsm_call_adapter: termination id=1 cause=16 result=accepted
gsm_call_adapter: termination id=1 cause=16 result=rejected
GSM service downlink kind=12 sapi=0 pd=03 message=07 length=2
GSM service uplink sapi=0 pd=03 message=0f length=2
gsm_session: outgoing termination consumed id=1 cause=16
GSM service downlink kind=13 sapi=0 pd=03 message=25 length=5
GSM service uplink sapi=0 pd=03 message=2d length=2
GSM service downlink kind=22 sapi=0 pd=03 message=2a length=2
LAPDm service Channel Release acknowledged
PCH no-identity fill
"""

ALERTING_TRACE = TRACE.replace(
    "gsm_call_adapter: decision id=1 outcome=0 result=accepted",
    "gsm_call_adapter: decision id=1 outcome=2 result=accepted",
).replace(
    "GSM service downlink kind=12 sapi=0 pd=03 message=07 length=2\n"
    "GSM service uplink sapi=0 pd=03 message=0f length=2",
    "GSM service downlink kind=11 sapi=0 pd=03 message=01 length=2",
)


class HostTerminationTraceCheckTest(unittest.TestCase):
    def test_accepts_complete_correlated_clear(self):
        check(TRACE)

    def test_rejects_missing_duplicate_rejection(self):
        with self.assertRaises(ValueError):
            check(TRACE.replace(
                "gsm_call_adapter: termination id=1 cause=16 result=rejected\n",
                "",
            ))

    def test_accepts_alerting_clear(self):
        check(ALERTING_TRACE, "alerting")

    def test_requires_requested_state_roundtrip(self):
        with self.assertRaises(ValueError):
            check(TRACE, require_state_roundtrip=True)


if __name__ == "__main__":
    unittest.main()
