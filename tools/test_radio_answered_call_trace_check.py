import unittest

from tools.radio_answered_call_trace_check import verify


PREFIX = """
dspif_transport: RX enqueue type=80 payload=34 producer=0ad data=801200001bc500010000036621062e0940010063012b2b
dsp_hle: TX packet type=02 payload=20 words=11 radio_phase=traffic_channel_change data=041202860271012fc12b0001002b00042b000000
dsp_hle: GSM service uplink sapi=0 pd=06 message=29 length=3
dsp_hle: GSM service uplink sapi=0 pd=03 message=07 length=2
dspif_transport: RX enqueue type=80 payload=34 producer=0af data=b0120000170a00010000036009030f2b2b
dsp_hle: TX packet type=1b payload=25 words=14 radio_phase=service_uplink_request data=00b00321012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b
"""
POLL = (
    "dsp_hle: TX packet type=1b payload=26 words=14 "
    "radio_phase=service_uplink_request "
    "data=00f00103012b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b00\n"
)
GOOD = PREFIX + POLL * 10


class AnsweredCallTraceCheckTest(unittest.TestCase):
    def test_answered_interval_is_classified(self):
        counts = verify(GOOD)
        self.assertEqual(10, counts["empty_tch_polls"])
        self.assertEqual(0, counts["external_polls"])
        self.assertEqual(0, counts["speech_or_codec_packets"])

    def test_known_external_poll_is_not_misclassified_as_speech(self):
        poll = (
            "dsp_hle: TX packet type=05 payload=18 words=10 "
            "data=1e020000000c01015f000f71020200d901c5\n")
        counts = verify(GOOD + poll)
        self.assertEqual(1, counts["external_polls"])

    def test_rejects_unknown_post_answer_packet_family(self):
        with self.assertRaisesRegex(ValueError, "packet types: 22"):
            verify(GOOD + "dsp_hle: TX packet type=22 payload=2 data=0000\n")

    def test_requires_stable_polling_interval(self):
        with self.assertRaisesRegex(ValueError, "stable answered interval"):
            verify(PREFIX + POLL)


if __name__ == "__main__":
    unittest.main()
