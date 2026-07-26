import unittest

from tools.radio_3310_speech_control_trace_check import verify


TRACE = """
dsp_hle: GSM service uplink sapi=0 pd=03 message=07 length=2 t=18.0
dsp_hle: doorbell pending=0000 wire=860b speech_control=060b t=18.1
dspif_transport: RX enqueue type=80 payload=34 data=b01200000f4600580000036009030f2b t=18.2
dsp_hle: speech tick uplink=1 downlink=0 pcm=1 pcm_clock=1000000/8000 pcm_shape=125 serial_clocks=20000/17280 t=18.3
dsp_hle: GSM service uplink sapi=0 pd=03 message=25 length=5 t=21.0
dsp_hle: speech stop control=060b uplink=135 downlink=133 t=21.05
dsp_hle: doorbell pending=0000 wire=840a speech_control=040a t=21.1
"""


class Radio3310SpeechControlTraceCheckTest(unittest.TestCase):
    def test_accepts_independent_pcm_lifecycle(self):
        verify(TRACE)

    def test_rejects_missing_release(self):
        with self.assertRaisesRegex(ValueError, "speech release"):
            verify(TRACE.rsplit("\n", 2)[0])

    def test_rejects_wrong_pcm_clock(self):
        with self.assertRaisesRegex(ValueError, "NHM-5 PCM transfer"):
            verify(TRACE.replace(
                "pcm_clock=1000000/8000 pcm_shape=125",
                "pcm_clock=520000/8000 pcm_shape=65"))


if __name__ == "__main__":
    unittest.main()
