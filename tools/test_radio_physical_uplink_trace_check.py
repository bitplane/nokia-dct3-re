from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from tools.radio_physical_uplink_trace_check import check


class RadioPhysicalUplinkTraceCheckTests(unittest.TestCase):
    def write_log(self, directory: str, mic_peak: int, nonzero: int,
                  peer_peak: int) -> Path:
        log = Path(directory) / "error.log"
        log.write_text("\n".join([
            "dsp_hle: speech tick uplink=1 downlink=0 pcm=1 "
            "mic_peak=1 ear_peak=0 nonzero=1/0",
            f"dsp_hle: speech tick uplink=150 downlink=149 pcm=150 "
            f"mic_peak={mic_peak} ear_peak=4096 nonzero={nonzero}/149",
            f"gsm_voice_peer: exchange=150 uplink_peak={peer_peak} "
            "downlink_peak=4096 source=lab-1khz",
        ]))
        return log

    def test_accepts_non_silent_physical_uplink_with_headroom(self):
        with TemporaryDirectory() as directory:
            result = check(self.write_log(directory, 16077, 150, 16224))
            self.assertIn("150/150", result)

    def test_rejects_silent_microphone(self):
        with TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "remained silent"):
                check(self.write_log(directory, 0, 0, 16))

    def test_rejects_clipped_microphone(self):
        with TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "clipped"):
                check(self.write_log(directory, 32768, 150, 32768))

    def test_uses_whole_call_peak_not_final_silence_floor(self):
        with TemporaryDirectory() as directory:
            log = self.write_log(directory, 1200, 150, 16)
            text = log.read_text()
            log.write_text(text.replace(
                "gsm_voice_peer: exchange=150",
                "gsm_voice_peer: exchange=100 uplink_peak=2032 "
                "downlink_peak=4096 source=lab-1khz\n"
                "gsm_voice_peer: exchange=150",
            ))
            self.assertIn("network peaks=1200/2032", check(log))

    def test_rejects_earlier_clipped_microphone_checkpoint(self):
        with TemporaryDirectory() as directory:
            log = self.write_log(directory, 1200, 150, 2032)
            text = log.read_text()
            log.write_text(text.replace(
                "dsp_hle: speech tick uplink=150",
                "dsp_hle: speech tick uplink=100 downlink=95 pcm=100 "
                "mic_peak=32768 ear_peak=4096 nonzero=100/95\n"
                "dsp_hle: speech tick uplink=150",
            ))
            with self.assertRaisesRegex(ValueError, "clipped"):
                check(log)


if __name__ == "__main__":
    unittest.main()
