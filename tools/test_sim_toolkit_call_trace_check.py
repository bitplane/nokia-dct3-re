import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sim_toolkit_call_trace_check as check


GOOD = "\n".join([
    "envelope data=d30702020181100101",
    "menu selection item=1 accepted",
    "proactive SET UP CALL ready",
    "SIM status ins=c2 sw=911c",
    "header cla=a0 ins=12 p1=00 p2=00 p3=1c",
    "GSM service uplink sapi=0 pd=03 message=05 length=15 "
    "data=03450401a05e0581551532f4150101",
    "GSM outgoing request id=1 digits=5551234",
    "GSM service downlink kind=10 sapi=0 pd=03 message=02 length=2",
    "GSM service downlink kind=14 sapi=0 pd=06 message=2e length=8",
    "GSM service uplink sapi=0 pd=06 message=29 length=3",
    "GSM service downlink kind=11 sapi=0 pd=03 message=01 length=2",
    "GSM service downlink kind=12 sapi=0 pd=03 message=07 length=2",
    "GSM service uplink sapi=0 pd=03 message=0f length=2",
    "terminal-response data=810306100002028281030100",
    "SIM status ins=14 sw=9000",
    "GSM service uplink sapi=0 pd=03 message=25 length=5",
    "GSM service downlink kind=25 sapi=0 pd=03 message=2d length=2",
    "GSM service uplink sapi=0 pd=03 message=2a length=2",
    "LAPDm service Channel Release acknowledged nr=4",
])


class SimToolkitCallTraceCheckTest(unittest.TestCase):
    def test_complete_lifecycle_passes(self):
        check.verify(GOOD)

    def test_wrong_number_fails(self):
        with self.assertRaisesRegex(ValueError, "destination 5551234"):
            check.verify(GOOD.replace("81551532f4", "81214365f7"))


if __name__ == "__main__":
    unittest.main()
