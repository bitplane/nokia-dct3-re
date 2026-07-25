import tempfile
import unittest
from pathlib import Path

from tools.radio_call_state_roundtrip_trace_check import check


GOOD = """
dsp_hle: speech tick uplink=1 downlink=0 pcm=1 t=27.240000
dsp_hle: speech tick uplink=50 downlink=44 pcm=50 t=28.220000
state_replay: phase=reference event=begin t=28.510000
dsp_hle: speech tick uplink=100 downlink=94 pcm=100 t=29.220000
gsm_voice_peer: exchange=100 uplink_peak=24 downlink_peak=4096 source=lab-1khz t=29.192308
state_replay: phase=reference event=end t=29.610000
state_replay: phase=restored event=begin t=28.510000
state_roundtrip: result=pass timer_delta=0000 mode=1234 requested_at=28.500000 t=28.510000
dsp_hle: speech tick uplink=100 downlink=94 pcm=100 t=29.220000
gsm_voice_peer: exchange=100 uplink_peak=24 downlink_peak=4096 source=lab-1khz t=29.192308
state_replay: phase=restored event=end t=29.610000
dsp_hle: speech tick uplink=150 downlink=144 pcm=150 t=30.220000
dsp_hle: speech stop control=060b uplink=174 downlink=168 t=30.720000
"""


class CallStateRoundtripTests(unittest.TestCase):
    def run_check(self, text):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "error.log"
            path.write_text(text)
            return check(path)

    def test_accepts_active_speech_roundtrip_and_teardown(self):
        result = self.run_check(GOOD)
        self.assertIn("174/168", result)
        self.assertIn("replayed 2", result)

    def test_rejects_failed_roundtrip(self):
        with self.assertRaisesRegex(ValueError, "successful"):
            self.run_check(GOOD.replace("result=pass", "result=fail"))

    def test_rejects_roundtrip_outside_speech(self):
        with self.assertRaisesRegex(ValueError, "bracketed"):
            self.run_check(
                GOOD.replace(
                    "requested_at=28.500000 t=28.510000",
                    "requested_at=26.000000 t=26.000000",
                )
            )

    def test_rejects_missing_teardown(self):
        with self.assertRaisesRegex(ValueError, "tear down"):
            self.run_check(GOOD.split("dsp_hle: speech stop", 1)[0])

    def test_rejects_replay_divergence(self):
        with self.assertRaisesRegex(ValueError, "diverged"):
            self.run_check(
                GOOD.replace(
                    "gsm_voice_peer: exchange=100 uplink_peak=24",
                    "gsm_voice_peer: exchange=100 uplink_peak=25",
                    1,
                )
            )


if __name__ == "__main__":
    unittest.main()
