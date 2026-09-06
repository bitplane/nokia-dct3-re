import tempfile
import unittest
from pathlib import Path

from tools.radio_handover_trace_check import check


SUCCESS = """
gsm_session: handover command target=2 reference=5a t=18.77
dsp_hle: receiver tuned old_arfcn=1 new_arfcn=2 t=18.78
dspif_transport: RX enqueue type=80 payload=34 data=b01200000fe80002000003030d062d002b t=18.79
dsp_hle: GSM service uplink sapi=0 pd=06 message=2c length=3 data=062c00 t=18.80
gsm_session: handover complete serving=2 t=18.80
dsp_hle: speech tick uplink=60 downlink=50 t=19.0
"""
FAILURE = """
gsm_session: handover command target=2 reference=5a t=18.77
dsp_hle: receiver tuned old_arfcn=1 new_arfcn=2 t=18.78
dsp_hle: receiver tuned old_arfcn=2 new_arfcn=1 t=19.09
gsm_session: handover rollback serving=1 t=19.10
dsp_hle: speech tick uplink=60 downlink=50 t=20.0
"""


class RadioHandoverTraceCheckTest(unittest.TestCase):
    def run_check(self, text: str, outcome: str) -> str:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "error.log"
            path.write_text(text)
            return check(path, outcome)

    def test_accepts_success(self):
        self.assertIn("ARFCN 2", self.run_check(SUCCESS, "success"))

    def test_accepts_failure_rollback(self):
        self.assertIn("ARFCN 1", self.run_check(FAILURE, "failure"))

    def test_rejects_reordered_success(self):
        reordered = SUCCESS.replace(
            "dsp_hle: receiver tuned old_arfcn=1 new_arfcn=2 t=18.78",
            "TARGET", 1).replace(
            "gsm_session: handover complete serving=2 t=18.80",
            "dsp_hle: receiver tuned old_arfcn=1 new_arfcn=2 t=18.78",
            1).replace("TARGET",
                "gsm_session: handover complete serving=2 t=18.80", 1)
        with self.assertRaisesRegex(ValueError, "out of order"):
            self.run_check(reordered, "success")

    def test_rejects_mixed_failure(self):
        with self.assertRaisesRegex(ValueError, "unexpectedly completed"):
            self.run_check(FAILURE + "gsm_session: handover complete serving=2\n",
                           "failure")

    def test_requires_roundtrip_inside_handover(self):
        state = ("state_roundtrip: result=pass timer_delta=0000 mode=0004 "
                 "requested_at=18.85 t=18.85\n")
        self.assertIn("ARFCN 1", self.run_check(
            FAILURE + state, "failure"))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "error.log"
            path.write_text(FAILURE + state)
            self.assertIn("ARFCN 1", check(path, "failure", True))

    def test_rejects_roundtrip_outside_handover(self):
        state = ("state_roundtrip: result=pass timer_delta=0000 mode=0004 "
                 "requested_at=19.20 t=19.20\n")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "error.log"
            path.write_text(FAILURE + state)
            with self.assertRaisesRegex(ValueError, "during handover"):
                check(path, "failure", True)


if __name__ == "__main__":
    unittest.main()
