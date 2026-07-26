from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from tools.radio_sacch_coexistence_trace_check import check


class RadioSacchCoexistenceTraceCheckTests(unittest.TestCase):
    def test_accepts_rotating_slots_inside_speech(self):
        with TemporaryDirectory() as directory:
            log = Path(directory) / "error.log"
            lines = [
                "dsp_hle: speech tick uplink=1 downlink=1 t=10.000000",
            ]
            for index in range(8):
                lines.append(
                    "radio_l1: kind=sacch "
                    f"slot={index + 1} phase={index & 3} "
                    "uplink_pending=0 downlink_pending=0 "
                    f"fn={25 + index * 26} t={10.100 + index * 0.120:.6f}"
                )
            lines.append(
                "dsp_hle: speech tick uplink=100 downlink=98 t=11.100000"
            )
            log.write_text("\n".join(lines))
            self.assertIn("8 SACCH/TF slots", check(log))

    def test_rejects_wrong_phase(self):
        with TemporaryDirectory() as directory:
            log = Path(directory) / "error.log"
            lines = ["dsp_hle: speech tick uplink=1 downlink=1 t=10.000000"]
            for index in range(8):
                phase = 1 if index == 4 else index & 3
                lines.append(
                    "radio_l1: kind=sacch "
                    f"slot={index + 1} phase={phase} "
                    "uplink_pending=0 downlink_pending=0 "
                    f"fn={25 + index * 26} t={10.100 + index * 0.120:.6f}"
                )
            lines.append(
                "dsp_hle: speech tick uplink=100 downlink=98 t=11.100000"
            )
            log.write_text("\n".join(lines))
            with self.assertRaisesRegex(ValueError, "phase"):
                check(log)


if __name__ == "__main__":
    unittest.main()
