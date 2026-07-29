import unittest

from tools.radio_outgoing_host_adapter_trace_check import check


TRACE = """
GSM outgoing request id=1 digits=5551234
gsm_call_adapter: request id=1 epoch=1 digits=5551234 clients=1
gsm_call_adapter: decision id=2 outcome=1 result=rejected
gsm_call_adapter: decision id=1 outcome=1 result=accepted
gsm_call_adapter: decision id=1 outcome=1 result=rejected
GSM service downlink kind=13 sapi=0 pd=03 message=25 length=5
LAPDm service Channel Release acknowledged nr=5
"""


class HostCallAdapterTraceTest(unittest.TestCase):
    def test_complete_trace(self):
        check(TRACE)

    def test_forbids_fallback(self):
        with self.assertRaisesRegex(ValueError, "fallback"):
            check(TRACE + "outgoing decision queued id=1 outcome=0")

    def test_requires_one_accepted_decision(self):
        with self.assertRaisesRegex(ValueError, "exactly one"):
            check(TRACE.replace(
                "gsm_call_adapter: decision id=1 outcome=1 result=accepted\n",
                "gsm_call_adapter: decision id=1 outcome=1 result=accepted\n" * 2,
            ))


if __name__ == "__main__":
    unittest.main()
