import unittest

from tools.radio_outgoing_host_two_calls_trace_check import check


CALL = """
GSM outgoing request id={request} digits=5551234
GSM service downlink kind=13 sapi=0 pd=03 message=25 length=5
LAPDm service Channel Release acknowledged
PCH no-identity fill
"""
TRACE = CALL.format(request=1) + """
gsm_call_adapter: decision id=1 outcome=0 result=rejected
gsm_call_adapter: decision id=2 outcome=1 result=accepted
""" + CALL.format(request=2)


class HostTwoCallsTraceCheckTest(unittest.TestCase):
    def test_accepts_two_isolated_calls(self):
        # Put correlation messages after the second request, as in live order.
        live = CALL.format(request=1) + (
            "GSM outgoing request id=2 digits=5551234\n"
            "gsm_call_adapter: decision id=1 outcome=0 result=rejected\n"
            "gsm_call_adapter: decision id=2 outcome=1 result=accepted\n"
            "GSM service downlink kind=13 sapi=0 pd=03 message=25 length=5\n"
            "LAPDm service Channel Release acknowledged\n"
            "PCH no-identity fill\n"
        )
        check(live)

    def test_rejects_one_teardown(self):
        with self.assertRaises(ValueError):
            check(CALL.format(request=1))


if __name__ == "__main__":
    unittest.main()
