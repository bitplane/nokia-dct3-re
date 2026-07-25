import tempfile
import unittest
from pathlib import Path

from tools.radio_pcm_missing_trace_check import check


GOOD = """
dsp_shared_control: command=08 value=060b commit=1 task=05 t=10.000000
dsp_hle: speech blocked by unsupported PCM link control=060b enabled=0 clock=520000/8000 shape=65 t=10.020000
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

    def test_rejects_codec_activity(self):
        with self.assertRaisesRegex(ValueError, "speech clock"):
            self.run_check(GOOD + "\ndsp_hle: speech tick uplink=1 downlink=0\n")

    def test_rejects_network_media(self):
        with self.assertRaisesRegex(ValueError, "network"):
            self.run_check(GOOD + "\ngsm_voice_peer: exchange=1\n")

    def test_rejects_missing_release(self):
        with self.assertRaisesRegex(ValueError, "Release Complete"):
            self.run_check(GOOD.split("dsp_hle: GSM service", 1)[0])


if __name__ == "__main__":
    unittest.main()
