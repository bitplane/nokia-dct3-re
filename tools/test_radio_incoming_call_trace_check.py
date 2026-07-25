import unittest

from tools.radio_incoming_call_trace_check import verify


GOOD = """
dsp_hle: LAPDm Channel Release acknowledged nr=2
dsp_hle: PCH IMSI page transmitted channel=60 fn=3759
dsp_hle: TX packet type=1b payload=25 words=14 radio_phase=contention_resolution data=0080013f4106270703331881080910101032547698
dspif_transport: RX enqueue type=80 payload=34 producer=0a8 data=8012000012c40001000001734106270703331881080910101032547698
dspif_transport: RX enqueue type=80 payload=34 producer=0ba data=8012000012c50001000003000d0635002b2b2b2b
dsp_hle: TX packet type=14 payload=12 words=7 radio_phase=service_uplink_request data=00f4ffffffffffffffff0000
dspif_transport: RX enqueue type=80 payload=34 producer=0c0 data=8012000012d500010000030229053247627042210000002b2b2b2b
dsp_hle: GSM service uplink sapi=0 pd=06 message=32 length=2
dspif_transport: RX enqueue type=80 payload=34 producer=0d1 data=8012000012f800010000032445030504046002008134015c0581551532f42b2b
dsp_hle: GSM service uplink sapi=0 pd=03 message=08 length=5
dspif_transport: RX enqueue type=80 payload=34 producer=0da data=80120000196000010000036621062e0940010063012b2b
dsp_hle: GSM service uplink sapi=0 pd=03 message=01 length=2
dsp_hle: TX packet type=02 payload=20 words=11 radio_phase=traffic_channel_change data=041202860271012fc12b0001002b00042b000000
dspif_transport: RX enqueue type=89 payload=8 producer=092 data=0000000000000000
dsp_hle: TX packet type=1b payload=25 words=14 radio_phase=traffic_contention_resolution data=00b0013f012b2b
dspif_transport: RX enqueue type=80 payload=34 producer=0a5 data=b01200001964000100000173012b2b
dsp_hle: GSM service uplink sapi=0 pd=06 message=29 length=3
dsp_hle: GSM service uplink sapi=0 pd=03 message=25 length=5
dspif_transport: RX enqueue type=80 payload=34 producer=0db data=b0120000196800010000036009032d2b2b2b
dsp_hle: GSM service uplink sapi=0 pd=03 message=2a length=2
dspif_transport: RX enqueue type=80 payload=34 producer=089 data=b0120000196a0001000003820d060d002b2b2b
dsp_hle: TX packet type=1b payload=25 words=14 radio_phase=traffic_release_acknowledgement data=00b00153012b2b
dspif_transport: RX enqueue type=80 payload=34 producer=09b data=b0120000196b000100000173012b2b
dsp_hle: TX packet type=02 payload=20 words=11 radio_phase=release_channel_change data=041202001117001a600000010000000800000001
dspif_transport: RX enqueue type=89 payload=8 producer=0a0 data=0000000000000000
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
            if "032445030504" in line)
        with self.assertRaisesRegex(ValueError, "exactly one incoming SETUP"):
            verify(GOOD + setup + "\n")


if __name__ == "__main__":
    unittest.main()
