import unittest

from tools.radio_outgoing_host_reconnect_trace_check import check


TRACE = """
gsm_call_adapter: request id=1 epoch=1 digits=5551234 clients=1
gsm_call_adapter: request id=1 epoch=1 digits=5551234 clients=1
gsm_call_adapter: state id=1 epoch=1 phase=connected
gsm_call_adapter: request id=1 epoch=1 digits=5551234 clients=1
state_roundtrip: result=pass
gsm_call_adapter: request id=1 epoch=2 digits=5551234 clients=1
gsm_call_adapter: state id=1 epoch=2 phase=connected
gsm_call_adapter: stale epoch=1 current=2 type=termination id=1 result=rejected
gsm_call_adapter: termination id=1 cause=16 result=accepted
GSM service downlink kind=13 sapi=0 pd=03 message=25 length=5
LAPDm service Channel Release acknowledged
PCH no-identity fill
"""

ALERTING_TRACE = """
gsm_call_adapter: request id=1 epoch=1 digits=5551234 clients=1
gsm_call_adapter: state id=1 epoch=1 phase=alerting
gsm_call_adapter: request id=1 epoch=1 digits=5551234 clients=1
gsm_call_adapter: state id=1 epoch=1 phase=alerting
gsm_call_adapter: termination id=1 cause=16 result=accepted
GSM service downlink kind=13 sapi=0 pd=03 message=25 length=5
LAPDm service Channel Release acknowledged
PCH no-identity fill
"""


class HostReconnectTraceCheckTest(unittest.TestCase):
    def test_accepts_complete_resynchronization(self):
        check(TRACE)

    def test_requires_all_reconnect_publications(self):
        with self.assertRaises(ValueError):
            check(TRACE.replace(
                "gsm_call_adapter: request id=1 epoch=1 "
                "digits=5551234 clients=1\n", "", 1))

    def test_accepts_alerting_reconnect(self):
        check(ALERTING_TRACE, "alerting")

    def test_requires_alerting_state_republication(self):
        with self.assertRaisesRegex(ValueError, "alerting state"):
            check(
                ALERTING_TRACE.replace(
                    "gsm_call_adapter: state id=1 epoch=1 phase=alerting\n",
                    "",
                    1,
                ),
                "alerting",
            )


if __name__ == "__main__":
    unittest.main()
