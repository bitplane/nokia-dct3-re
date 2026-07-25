from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from tools.radio_degraded_speech_trace_check import check


class RadioDegradedSpeechTraceCheckTests(unittest.TestCase):
    def test_accepts_bad_frame_and_recovery(self):
        with TemporaryDirectory() as directory:
            log = Path(directory) / "error.log"
            lines = [
                "dsp_shared_control: command=08 value=060b commit=1 t=9.990000",
                "dsp_hle: speech tick uplink=1 downlink=0 pcm=1 pcm_clock=520000/8000 pcm_shape=65 serial_clocks=10400/7680 mic_peak=0 ear_peak=0 nonzero=0/0 concealed=1 muted=0 t=10.000000",
                "gsm_voice_peer: exchange=1 uplink_peak=16 downlink_peak=4096 source=lab-1khz uplink_good=1 concealed=0 muted=0 t=10.001000",
                "dsp_hle: speech tick uplink=2 downlink=1 pcm=2 pcm_clock=520000/8000 pcm_shape=65 serial_clocks=20800/15360 mic_peak=0 ear_peak=4096 nonzero=0/1 concealed=1 muted=0 t=10.020000",
                "dsp_hle: speech tick uplink=3 downlink=2 pcm=3 pcm_clock=520000/8000 pcm_shape=65 serial_clocks=31200/23040 mic_peak=0 ear_peak=4096 nonzero=0/2 concealed=1 muted=0 t=10.040000",
                "radio_l1: direction=uplink impairment=invert-data burst=24 count=1 fn=100 t=10.100000",
                "radio_l1: direction=downlink impairment=invert-data burst=24 count=1 fn=100 t=10.100000",
                "radio_l1: direction=uplink kind=speech good=0 count=1 fn=104 t=10.120000",
                "radio_l1: direction=downlink kind=speech good=0 count=1 fn=104 t=10.120000",
                "radio_l1: direction=downlink kind=facch good=1 count=1 fn=120 t=10.200000",
                "dsp_hle: speech tick uplink=50 downlink=47 pcm=50 pcm_clock=520000/8000 pcm_shape=65 serial_clocks=520000/384000 mic_peak=0 ear_peak=2048 nonzero=0/48 concealed=3 muted=0 t=11.000000",
                "gsm_voice_peer: exchange=102 uplink_peak=16 downlink_peak=4096 source=lab-1khz uplink_good=1 concealed=1 muted=0 t=11.999000",
                "dsp_hle: speech tick uplink=102 downlink=100 pcm=102 pcm_clock=520000/8000 pcm_shape=65 serial_clocks=1060800/783360 mic_peak=0 ear_peak=4096 nonzero=0/101 concealed=3 muted=0 t=12.000000",
                "dsp_hle: speech tick uplink=103 downlink=101 pcm=103 pcm_clock=520000/8000 pcm_shape=65 serial_clocks=1071200/791040 mic_peak=0 ear_peak=4096 nonzero=0/102 concealed=3 muted=0 t=12.020000",
            ]
            log.write_text("\n".join(lines))
            self.assertIn("observed 1", check(log))


if __name__ == "__main__":
    unittest.main()
