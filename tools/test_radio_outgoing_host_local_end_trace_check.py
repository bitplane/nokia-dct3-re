import unittest

from tools.radio_outgoing_host_local_end_trace_check import check


GOOD = """
GSM service uplink sapi=0 pd=03 message=0f length=2
GSM service uplink sapi=0 pd=03 message=25 length=4
GSM service downlink kind=1 sapi=0 pd=03 message=2d length=2
LAPDm service Channel Release acknowledged
PCH no-identity fill
gsm_call_adapter: media direction=downlink id=1 sequence=0 result=rejected
gsm_call_adapter: termination id=1 cause=16 result=rejected
"""


class HostLocalEndTraceCheckTests(unittest.TestCase):
    def test_accepts_stale_event_isolation(self):
        check(GOOD)

    def test_rejects_network_clear(self):
        with self.assertRaisesRegex(ValueError, "network Disconnect"):
            check(
                GOOD
                + "\nGSM service downlink kind=1 sapi=0 pd=03 "
                "message=25 length=5\n"
            )


if __name__ == "__main__":
    unittest.main()
