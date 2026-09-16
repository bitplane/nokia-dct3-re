import unittest

from tools.radio_5210_incoming_call_trace_check import verify


GOOD = """
LAPDm Channel Release acknowledged nr=2
PCH IMSI page transmitted channel=60 fn=2841
TX packet type=1b data=0080013f410627
RX enqueue type=80 payload=34 data=800000000cc7000100000173410627
RX enqueue type=80 payload=34 data=800000000cc90001000003000d063500
TX packet type=14 payload=12 data=00ebffffffffffffffff0000
RX enqueue type=80 payload=34 data=800000000ccb0001000003022905324762704221000000
GSM service uplink sapi=0 pd=06 message=32 length=2
RX enqueue type=80 payload=34 data=800000000ccd00010000032445030504046002008134015c0581551532f4
GSM service uplink sapi=0 pd=03 message=08 length=5
GSM service uplink sapi=0 pd=03 message=01 length=2
TX packet type=02 payload=20 data=040002000271012fc10000010000000400000000
TX packet type=1b data=00b0013f01
RX enqueue type=80 payload=34 data=b00000000cdd00010000017301
GSM service uplink sapi=0 pd=06 message=29 length=3
GSM service uplink sapi=0 pd=03 message=07 length=2
dsp_hle: doorbell pending=0000 wire=860b speech_control=060b
RX enqueue type=80 payload=34 data=b00000000f4300010000036009030f
GSM service uplink sapi=0 pd=03 message=25 length=5
RX enqueue type=80 payload=34 data=b0000000123800010000038209032d
GSM service uplink sapi=0 pd=03 message=2a length=2
RX enqueue type=80 payload=34 data=b0000000123b0001000003a40d060d00
RX enqueue type=80 payload=34 data=b0000000123d00010000017301
TX packet type=02 payload=20 data=040000001117001a600000560000001400000001
RX enqueue type=89 payload=8 data=0000000000000000
dsp_hle: doorbell pending=0000 wire=840a speech_control=040a
RX enqueue type=80 payload=34 data=600000001311000100001506210001f0
"""


class Radio5210IncomingCallTraceCheckTest(unittest.TestCase):
    def test_accepts_complete_lifecycle(self):
        verify(GOOD)

    def test_rejects_foreign_cipher_control(self):
        with self.assertRaisesRegex(ValueError, "cipher-control"):
            verify(GOOD.replace("00ebffffffff", "00f4ffffffff"))

    def test_accepts_redacted_a5_1_control(self):
        verify(GOOD.replace(
            "data=00ebffffffffffffffff0000", "data=<redacted>"), a5_1=True)

    def test_rejects_missing_speech_release(self):
        with self.assertRaisesRegex(ValueError, "speech release"):
            verify(GOOD.replace(
                "dsp_hle: doorbell pending=0000 wire=840a speech_control=040a\n",
                ""))

    def test_rejects_duplicate_connect(self):
        with self.assertRaisesRegex(ValueError, "exactly one Connect"):
            verify(GOOD + "GSM service uplink sapi=0 pd=03 message=07 length=2\n")


if __name__ == "__main__":
    unittest.main()
