from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from tools.radio_speech_media_trace_check import check


class RadioSpeechMediaTraceCheckTests(unittest.TestCase):
    def test_accepts_bidirectional_twenty_ms_media(self):
        with TemporaryDirectory() as directory:
            log = Path(directory) / "error.log"
            log.write_text("\n".join([
                "dsp_shared_control: command=08 value=060b commit=1 t=9.990000",
                "dsp_hle: speech tick uplink=1 downlink=0 pcm=1 pcm_clock=520000/8000 pcm_shape=65 serial_clocks=10400/7680 mic_peak=0 ear_peak=0 nonzero=0/0 t=10.000000",
                "gsm_voice_peer: exchange=1 uplink_peak=16 downlink_peak=4096 source=lab-1khz t=10.001000",
                "dsp_hle: speech tick uplink=2 downlink=1 pcm=2 pcm_clock=520000/8000 pcm_shape=65 serial_clocks=20800/15360 mic_peak=0 ear_peak=4096 nonzero=0/1 t=10.020000",
                "dsp_hle: speech tick uplink=3 downlink=2 pcm=3 pcm_clock=520000/8000 pcm_shape=65 serial_clocks=31200/23040 mic_peak=0 ear_peak=4096 nonzero=0/2 t=10.040000",
                "dsp_hle: speech tick uplink=50 downlink=48 pcm=50 pcm_clock=520000/8000 pcm_shape=65 serial_clocks=520000/384000 mic_peak=0 ear_peak=4096 nonzero=0/48 t=11.000000",
                "gsm_voice_peer: exchange=102 uplink_peak=16 downlink_peak=4096 source=lab-1khz t=11.999000",
                "dsp_hle: speech tick uplink=102 downlink=100 pcm=102 pcm_clock=520000/8000 pcm_shape=65 serial_clocks=1060800/783360 mic_peak=0 ear_peak=4096 nonzero=0/100 t=12.000000",
                "dsp_shared_control: command=08 value=040a commit=1 t=12.010000",
                "dsp_hle: speech stop control=040a uplink=102 downlink=100 t=12.020000",
            ]))
            self.assertIn("102 encoded", check(log))

    def test_rejects_one_way_media(self):
        with TemporaryDirectory() as directory:
            log = Path(directory) / "error.log"
            log.write_text("\n".join([
                "dsp_shared_control: command=08 value=060b commit=1 t=9.990000",
                "dsp_hle: speech tick uplink=1 downlink=0 pcm=1 pcm_clock=520000/8000 pcm_shape=65 serial_clocks=10400/7680 mic_peak=0 ear_peak=0 nonzero=0/0 t=10.000000",
                "dsp_hle: speech tick uplink=2 downlink=0 pcm=2 pcm_clock=520000/8000 pcm_shape=65 serial_clocks=20800/15360 mic_peak=0 ear_peak=0 nonzero=0/0 t=10.020000",
                "dsp_hle: speech tick uplink=3 downlink=0 pcm=3 pcm_clock=520000/8000 pcm_shape=65 serial_clocks=31200/23040 mic_peak=0 ear_peak=0 nonzero=0/0 t=10.040000",
                "dsp_hle: speech tick uplink=50 downlink=0 pcm=50 pcm_clock=520000/8000 pcm_shape=65 serial_clocks=520000/384000 mic_peak=0 ear_peak=0 nonzero=0/0 t=11.000000",
                "dsp_hle: speech tick uplink=100 downlink=0 pcm=100 pcm_clock=520000/8000 pcm_shape=65 serial_clocks=1040000/768000 mic_peak=0 ear_peak=0 nonzero=0/0 t=12.000000",
            ]))
            with self.assertRaisesRegex(ValueError, "bidirectional"):
                check(log)


if __name__ == "__main__":
    unittest.main()
