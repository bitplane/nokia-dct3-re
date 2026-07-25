import tempfile
import unittest
from pathlib import Path

from tools.radio_pcm_missing_trace_check import check


GOOD = """
dsp_shared_control: command=08 value=060b commit=1 task=05 t=10.000000
dsp_hle: speech blocked by unsupported PCM link control=060b enabled=0 clock=520000/8000 shape=65 t=10.020000
gsm_voice_peer: exchange=1 uplink_peak=0 downlink_peak=4096 source=lab-1khz uplink_good=0 concealed=1 muted=0 t=10.030000
dsp_hle: GSM service uplink sapi=0 pd=03 message=2a length=2 t=13.000000
"""


class MissingPcmTraceTests(unittest.TestCase):
    def run_check(self, text):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "error.log"
            path.write_text(text)
            return check(path)

    def test_accepts_control_only_call(self):
        self.assertIn("no codec", self.run_check(GOOD))

    def test_accepts_v501_command_tagged_wire(self):
        self.assertIn("no codec", self.run_check(GOOD.replace(
            "value=060b", "value=860b"
        )))

    def test_correlates_speech_request_after_idle_control(self):
        self.assertIn("no codec", self.run_check(
            GOOD.replace(
                "dsp_shared_control:",
                "dsp_shared_control: command=08 value=0002 commit=1 "
                "task=05 t=9.000000\ndsp_shared_control:",
                1,
            )
        ))

    def test_rejects_wire_control_mismatch(self):
        with self.assertRaisesRegex(ValueError, "did not reach DSP control"):
            self.run_check(GOOD.replace("control=060b", "control=060a"))

    def test_rejects_codec_activity(self):
        with self.assertRaisesRegex(ValueError, "speech clock"):
            self.run_check(GOOD + "\ndsp_hle: speech tick uplink=1 downlink=0\n")

    def test_rejects_good_network_uplink(self):
        with self.assertRaisesRegex(ValueError, "good uplink"):
            self.run_check(GOOD.replace("uplink_good=0", "uplink_good=1"))

    def test_rejects_missing_independent_downlink(self):
        with self.assertRaisesRegex(ValueError, "downlink"):
            self.run_check("\n".join(
                line for line in GOOD.splitlines()
                if "gsm_voice_peer: exchange=" not in line
            ))

    def test_rejects_missing_release(self):
        with self.assertRaisesRegex(ValueError, "Release Complete"):
            self.run_check(GOOD.split("dsp_hle: GSM service", 1)[0])


if __name__ == "__main__":
    unittest.main()
