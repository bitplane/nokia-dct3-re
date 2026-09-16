import unittest

from tools.radio_5210_outgoing_call_trace_check import verify


GOOD = """
dsp_hle: doorbell pending=0000 wire=860b speech_control=060b
GSM service establish sapi=0 pd=05 message=24 length=16 data=05247103335981080910101032547698
GSM service downlink kind=5 sapi=0 pd=05 message=21 length=2
GSM service uplink sapi=0 pd=03 message=05 length=15 data=03450401a05e0581551532f4150101
GSM service downlink kind=10 sapi=0 pd=03 message=02 length=2
GSM service downlink kind=14 sapi=0 pd=06 message=2e length=8
TX packet type=02 payload=20 data=040002000271012fc10000010000000400000000
GSM service uplink sapi=0 pd=06 message=29 length=3 data=062900
GSM service downlink kind=11 sapi=0 pd=03 message=01 length=2
GSM service downlink kind=12 sapi=0 pd=03 message=07 length=2
GSM service uplink sapi=0 pd=03 message=0f length=2 data=030f
GSM service uplink sapi=0 pd=03 message=25 length=5 data=036502e090
GSM service downlink kind=25 sapi=0 pd=03 message=2d length=2
GSM service uplink sapi=0 pd=03 message=2a length=2 data=032a
LAPDm service Channel Release acknowledged nr=4
dsp_hle: doorbell pending=0000 wire=840a speech_control=040a
TX packet type=02 payload=20 data=040000001117001a600000560000001400000001
RX enqueue type=89 payload=8 data=0000000000000000
PCH no-identity fill channel=60 fn=6309
"""


class Radio5210OutgoingCallTraceCheckTest(unittest.TestCase):
    def test_accepts_complete_lifecycle(self):
        verify(GOOD)

    def test_rejects_foreign_channel_configuration(self):
        with self.assertRaisesRegex(ValueError, "traffic-channel configuration"):
            verify(GOOD.replace("040002000271012fc1", "040002000271012fc2"))

    def test_rejects_wrong_called_number(self):
        with self.assertRaisesRegex(ValueError, "expected"):
            verify(GOOD, "123")

    def test_rejects_missing_speech_release(self):
        with self.assertRaisesRegex(ValueError, "speech release"):
            verify(GOOD.replace(
                "dsp_hle: doorbell pending=0000 wire=840a speech_control=040a\n",
                ""))


if __name__ == "__main__":
    unittest.main()
