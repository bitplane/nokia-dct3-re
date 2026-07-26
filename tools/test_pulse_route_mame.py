import unittest
from pathlib import Path

from tools.pulse_route_mame import is_mame_stream


ROOT = Path(__file__).resolve().parents[1]


class PulseRouteMameTests(unittest.TestCase):
    def test_matches_process_binary(self):
        self.assertTrue(
            is_mame_stream(
                {"properties": {"application.process.binary": "mame"}}
            )
        )

    def test_matches_application_name_case_insensitively(self):
        self.assertTrue(
            is_mame_stream({"properties": {"application.name": "MAME"}})
        )

    def test_rejects_other_audio_clients(self):
        self.assertFalse(
            is_mame_stream(
                {
                    "properties": {
                        "application.process.binary": "ffmpeg",
                        "application.name": "Lavf",
                    }
                }
            )
        )

    def test_missing_properties_are_not_mame(self):
        self.assertFalse(is_mame_stream({}))

    def test_physical_gate_uses_separate_explicit_pulse_devices(self):
        script = (ROOT / "tools/run_physical_uplink_gate.sh").read_text()
        self.assertIn(
            '-device "$input_sink_name" -f pulse -', script
        )
        self.assertNotIn('-f pulse "$input_sink_name"', script)
        self.assertIn('-i "$output_sink_name.monitor"', script)
        self.assertIn(
            '--source "$input_sink_name.monitor" --sink "$output_sink_name"',
            script,
        )
        self.assertIn('pactl set-default-sink "$default_sink"', script)
        self.assertIn('pactl set-default-source "$default_source"', script)


if __name__ == "__main__":
    unittest.main()
