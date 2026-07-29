import tempfile
import unittest
from pathlib import Path

from tools.power_lifecycle_check import check, parse_summary


class PowerLifecycleCheckTest(unittest.TestCase):
    def test_short_press_stays_interactive(self):
        values = {
            "final_startup_mode": "0004",
            "final_startup_event": "0033",
            "final_sim_enable": "01",
            "startup_modes": "0001,0004,000D",
        }
        self.assertEqual(check("short", values), [])

    def test_long_press_reaches_shutdown(self):
        values = {
            "final_startup_mode": "000C",
            "final_startup_event": "0074",
            "final_sim_enable": "00",
            "startup_modes": "0001,0004,000C,000D",
        }
        self.assertEqual(
            check("long", values, "ccont_power: event=off t=20.011688538"),
            [],
        )

    def test_short_press_rejects_rail_off(self):
        values = {
            "final_startup_mode": "0004",
            "final_startup_event": "0033",
            "final_sim_enable": "01",
            "startup_modes": "0001,0004,000D",
        }
        self.assertIn(
            "short press removed the digital-baseband rail",
            check("short", values, "ccont_power: event=off t=18.0"),
        )

    def test_long_press_accepts_frozen_sim_ram_after_rail_off(self):
        values = {
            "final_startup_mode": "000C",
            "final_startup_event": "0074",
            "final_sim_enable": "01",
            "startup_modes": "0001,0004,000C,000D",
        }
        self.assertEqual(
            check("long", values, "ccont_power: event=off t=20.0"), []
        )

    def test_long_press_requires_rail_off(self):
        values = {
            "final_startup_mode": "000C",
            "final_startup_event": "0074",
            "final_sim_enable": "01",
            "startup_modes": "0001,0004,000C,000D",
        }
        self.assertIn(
            "long press did not remove the digital-baseband rail",
            check("long", values),
        )

    def test_parser_reads_key_values(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "summary.txt"
            path.write_text("final_startup_mode=000C\n")
            self.assertEqual(parse_summary(path)["final_startup_mode"], "000C")


if __name__ == "__main__":
    unittest.main()
