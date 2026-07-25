from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from tools.radio_facch_interruption_trace_check import check


GOOD = """
dsp_shared_control: command=08 value=060b commit=1 t=10.000000
dsp_hle: speech tick uplink=1 downlink=0 ear_peak=0 concealed=0 t=10.010000
gsm_voice_peer: exchange=1 uplink_peak=0 downlink_peak=4096 source=lab-1khz uplink_good=0 concealed=0 muted=0 t=10.011000
radio_l1: direction=uplink kind=facch good=1 count=1 fn=100 t=10.020000
radio_l1: direction=downlink kind=facch good=1 count=1 fn=100 t=10.020000
dsp_hle: speech tick uplink=2 downlink=0 ear_peak=0 concealed=1 t=10.030000
gsm_voice_peer: exchange=2 uplink_peak=0 downlink_peak=4096 source=lab-1khz uplink_good=0 concealed=1 muted=0 t=10.031000
dsp_hle: speech tick uplink=150 downlink=145 ear_peak=4096 concealed=1 t=13.000000
gsm_voice_peer: exchange=150 uplink_peak=16 downlink_peak=4096 source=lab-1khz uplink_good=1 concealed=1 muted=0 t=13.001000
dsp_hle: speech stop control=040a uplink=150 downlink=145 t=14.000000
"""


class RadioFacchInterruptionTraceCheckTests(unittest.TestCase):
    def run_check(self, text):
        with TemporaryDirectory() as directory:
            path = Path(directory) / "error.log"
            path.write_text(text)
            return check(path)

    def test_accepts_bidirectional_facch_concealment_and_recovery(self):
        self.assertIn("recovered", self.run_check(GOOD))

    def test_rejects_facch_before_speech_route(self):
        with self.assertRaisesRegex(ValueError, "while speech was active"):
            self.run_check(GOOD.replace("t=10.020000", "t=9.990000"))

    def test_rejects_missing_handset_concealment(self):
        with self.assertRaisesRegex(ValueError, "handset BFI"):
            self.run_check(GOOD.replace(
                "downlink=0 ear_peak=0 concealed=1 t=10.030000",
                "downlink=0 ear_peak=0 concealed=0 t=10.030000",
            ))

    def test_rejects_missing_network_recovery(self):
        with self.assertRaisesRegex(ValueError, "network speech"):
            self.run_check(GOOD.replace(
                "exchange=150 uplink_peak=16",
                "exchange=150 uplink_peak=0",
            ))


if __name__ == "__main__":
    unittest.main()
