import unittest

from tools.radio_incoming_call_trace_check import verify


GOOD = """
dsp_hle: LAPDm Channel Release acknowledged nr=2
dsp_hle: PCH IMSI page transmitted channel=60 fn=3759
dsp_hle: TX packet type=1b payload=25 words=14 radio_phase=contention_resolution data=0080013f4106270703331881080910101032547698
dspif_transport: RX enqueue type=80 payload=34 producer=0a8 data=8012000012c40001000001734106270703331881080910101032547698
dspif_transport: RX enqueue type=80 payload=34 producer=0ba data=8012000012c500010000030029053247627042210000002b2b2b2b
dspif_transport: RX enqueue type=80 payload=34 producer=0d1 data=8012000012f800010000030245030504046002008134015c0581551532f42b2b
dsp_hle: GSM service uplink sapi=0 pd=03 message=08 length=5
dsp_hle: GSM service uplink sapi=0 pd=03 message=01 length=2
dsp_hle: GSM service uplink sapi=0 pd=03 message=25 length=5
dspif_transport: RX enqueue type=80 payload=34 producer=0db data=80120000130400010000038409032d2b2b2b
dspif_transport: RX enqueue type=80 payload=34 producer=0dc data=8012000013050001000003860d060d002b2b2b
dsp_hle: GSM service uplink sapi=0 pd=03 message=2a length=2
dsp_hle: LAPDm service Channel Release acknowledged nr=4
dspif_transport: RX enqueue type=80 payload=34 producer=085 data=6012000013dd000100001506210001f02b2b
"""


class IncomingCallTraceCheckTest(unittest.TestCase):
    def test_complete_bounded_call_attempt(self):
        verify(GOOD)

    def test_rejects_setup_without_mm_information(self):
        without = "\n".join(
            line for line in GOOD.splitlines()
            if "05324762704221000000" not in line)
        with self.assertRaisesRegex(ValueError, "MM Information"):
            verify(without)

    def test_rejects_missing_alerting(self):
        without = "\n".join(
            line for line in GOOD.splitlines()
            if "message=01" not in line)
        with self.assertRaisesRegex(ValueError, "Alerting"):
            verify(without)

    def test_rejects_duplicate_setup(self):
        setup = next(
            line for line in GOOD.splitlines()
            if "030245030504" in line)
        with self.assertRaisesRegex(ValueError, "exactly one incoming SETUP"):
            verify(GOOD + setup + "\n")


if __name__ == "__main__":
    unittest.main()
