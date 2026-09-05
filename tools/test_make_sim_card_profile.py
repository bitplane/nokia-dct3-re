import tempfile
import unittest
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import make_sim_card_profile as profile


class SimCardProfileTest(unittest.TestCase):
    def test_default_layout_matches_device_nvram_contract(self):
        data = profile.make_profile(False)
        self.assertEqual(3524, len(data))
        self.assertEqual(profile.CHV1 + profile.CHV2, data[3484:3500])
        self.assertEqual(profile.PUK1 + profile.PUK2, data[3500:3516])
        self.assertEqual(bytes((3, 3, 10, 10, 0)), data[3516:3521])
        self.assertEqual(b"\x00\x00\x00", data[3521:3524])

    def test_pin_profile_changes_only_persistent_enable_state(self):
        disabled = profile.make_profile(False)
        enabled = profile.make_profile(True)
        changed = [index for index, pair in enumerate(zip(disabled, enabled))
                   if pair[0] != pair[1]]
        self.assertEqual([3520], changed)
        self.assertEqual(1, enabled[3520])

    def test_cli_writes_parent_directories(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "nested" / "sim_card"
            old_argv = __import__("sys").argv
            try:
                __import__("sys").argv = ["make_sim_card_profile.py", "--output", str(output), "--pin-enabled"]
                self.assertEqual(0, profile.main())
            finally:
                __import__("sys").argv = old_argv
            self.assertEqual(profile.make_profile(True), output.read_bytes())


if __name__ == "__main__":
    unittest.main()
