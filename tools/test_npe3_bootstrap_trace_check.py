import unittest
from pathlib import Path
import tempfile

from tools.npe3_bootstrap_trace_check import check, check_frame


class Npe3BootstrapTest(unittest.TestCase):
    def setUp(self):
        self.text = (
            "gensio: W off=2d data=22 old=22 pc=004ec7b0\n"
            "gensio: R off=6d data=07 pc=004ec7bc\n" + "".join(
                f"dspif_transport: RAM W off={offset:03x} data=0000 t=0.5\n"
                for offset in [0xfe, 0x100] * 116))
        self.summary = {"final_pc": "00426CC8", "soft_resets": "0"}
        self.display = "display_io: off=6e data=24 old=00 pc=004e6564\n"
        for bank in range(8):
            self.display += f"display_io: off=6e data={0x40 | bank:02x} old=00 pc=004e657a\n"
            self.display += "display_io: off=6e data=80 old=00 pc=004e6582\n"
            self.display += "display_io: off=2e data=00 old=00 pc=004e658c\n" * 96
        self.display += "display_io: off=6e data=20 old=00 pc=004e65a6\n"
        self.text += self.display

    def test_frontier(self):
        check(self.text, self.summary)

    def test_missing_handoff(self):
        with self.assertRaises(ValueError):
            check(self.text.replace("dspif_transport: RAM W off=0fe data=0000 t=0.5\n", "", 1), self.summary)

    def test_wrong_receive_ready(self):
        with self.assertRaises(ValueError):
            check(self.text.replace("data=07", "data=03"), self.summary)

    def test_fabricated_completion(self):
        with self.assertRaises(ValueError):
            check(self.text + "bootstrap completion", self.summary)

    def test_wrong_frontier(self):
        with self.assertRaises(ValueError):
            check(self.text, {**self.summary, "final_pc": "004DC0FC"})

    def test_reset(self):
        with self.assertRaises(ValueError):
            check(self.text, {**self.summary, "soft_resets": "1"})

    def test_missing_bank(self):
        with self.assertRaisesRegex(ValueError, "LCD"):
            check(self.text.replace("data=47", "data=46"), self.summary)

    def test_missing_clear_byte(self):
        with self.assertRaisesRegex(ValueError, "LCD"):
            check(self.text.replace("display_io: off=2e data=00 old=00 pc=004e658c\n", "", 1), self.summary)

    def test_frame_geometry_and_frontier(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "frame.pgm"
            path.write_bytes(b"P5\n96 60\n255\n" + bytes([255]) * (96 * 60))
            check_frame(path)
            path.write_bytes(b"P5\n84 48\n255\n" + bytes([255]) * (84 * 48))
            with self.assertRaisesRegex(ValueError, "96x60"):
                check_frame(path)
            path.write_bytes(b"P5\n96 60\n255\n" + bytes([0]) * (96 * 60))
            with self.assertRaisesRegex(ValueError, "reassess"):
                check_frame(path)


if __name__ == "__main__":
    unittest.main()
