import unittest

from tools.radio_3310_speech_control_trace_check import verify


TRACE = """
dsp_hle: GSM service uplink sapi=0 pd=03 message=07 length=2 t=18.0
dsp_hle: doorbell pending=0000 wire=860b speech_control=060b t=18.1
dspif_transport: RX enqueue type=80 payload=34 data=b01200000f4600580000036009030f2b t=18.2
dsp_hle: GSM service uplink sapi=0 pd=03 message=25 length=5 t=21.0
dsp_hle: doorbell pending=0000 wire=840a speech_control=040a t=21.1
"""


class Radio3310SpeechControlTraceCheckTest(unittest.TestCase):
    def test_accepts_control_only_lifecycle(self):
        verify(TRACE)

    def test_rejects_missing_release(self):
        with self.assertRaisesRegex(ValueError, "speech release"):
            verify(TRACE.rsplit("\n", 2)[0])

    def test_rejects_unproved_media(self):
        with self.assertRaisesRegex(ValueError, "unsupported PCM profile"):
            verify(TRACE + "dsp_hle: speech tick uplink=1 downlink=0\n")


if __name__ == "__main__":
    unittest.main()
