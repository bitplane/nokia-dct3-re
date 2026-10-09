import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sim_toolkit_sms_trace_check as check


GOOD = "\n".join([
    "envelope data=d30702020181100101",
    "menu selection item=1 accepted",
    "proactive SEND SHORT MESSAGE ready",
    "SIM status ins=c2 sw=9124",
    "header cla=a0 ins=12 p1=00 p2=00 p3=24",
    "GSM service uplink sapi=3 pd=09 message=01 length=28 "
    "data=290119000100069121436587090e01000781551532f4000403534154",
    "gsm_sms_submit: cp=29 rp=01 smsc=1234567890 destination=5551234 "
    "alphabet=1 user_length=3 outcome=0 status_report=0",
    "GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2",
    "GSM service downlink kind=18 sapi=3 pd=09 message=01 length=5",
    "terminal-response data=810305130002028281030100",
    "SIM status ins=14 sw=9000",
    "GSM service uplink sapi=3 pd=09 message=04 length=2 data=2904",
    "LAPDm service Channel Release acknowledged nr=2",
])


class SimToolkitSmsTraceCheckTest(unittest.TestCase):
    def test_complete_lifecycle_passes(self):
        check.verify(GOOD)

    def primary_trace(self):
        return GOOD.replace('data=2901', 'data=3901').replace('cp=29', 'cp=39').replace(
            'data=2904', 'data=3904').replace('090e01000781', '090e01010781')

    def test_primary_identifiers(self):
        check.verify(self.primary_trace(), cp=0x39, message_reference=1)

    def test_wrong_primary_identifiers_or_payload_rejected(self):
        for text in (GOOD, self.primary_trace().replace('cp=39', 'cp=29'),
                     self.primary_trace().replace('3534154', '3534155')):
            with self.assertRaises(ValueError):
                check.verify(text, cp=0x39, message_reference=1)

    def test_missing_network_ack_fails(self):
        with self.assertRaisesRegex(ValueError, "downlink kind=18"):
            check.verify(GOOD.replace(
                "GSM service downlink kind=18 sapi=3 pd=09 message=01 length=5",
                "GSM service downlink kind=19 sapi=3 pd=09 message=01 length=5"))

    def test_duplicate_submit_fails(self):
        with self.assertRaisesRegex(ValueError, "exactly one"):
            check.verify(GOOD + "\ngsm_sms_submit:")


if __name__ == "__main__":
    unittest.main()
