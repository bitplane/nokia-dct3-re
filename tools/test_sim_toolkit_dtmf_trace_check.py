import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sim_toolkit_dtmf_trace_check as check


GOOD = "\n".join([
    "proactive SET UP CALL ready",
    "GSM outgoing request id=1 digits=5551234",
    "GSM service uplink sapi=0 pd=03 message=0f length=2 data=030f",
    "terminal-response data=810306100002028281030100",
    "proactive SEND DTMF ready",
    "SIM status ins=14 sw=910f",
    "terminal-response data=81030d140002028281030131",
    "SIM status ins=14 sw=9000",
    "GSM service uplink sapi=0 pd=03 message=25 length=5",
    "GSM service uplink sapi=0 pd=03 message=2a length=2 data=032a",
])


class SimToolkitDtmfTraceCheckTest(unittest.TestCase):
    def test_active_call_rejection_passes(self):
        check.verify(GOOD)

    def test_success_result_is_not_claimed(self):
        with self.assertRaisesRegex(ValueError, "0131"):
            check.verify(GOOD.replace("030131", "030100"))


if __name__ == "__main__":
    unittest.main()
