import unittest
from pathlib import Path
import tempfile
from PIL import Image

from tools.noki8850_outgoing_call_check import verify, verify_frames
from tools.test_radio_5210_outgoing_call_trace_check import GOOD as NSM5_GOOD


GOOD = NSM5_GOOD.replace(
    "dsp_hle: doorbell pending=0000 wire=860b speech_control=060b",
    "8850_call_physical: action=send\n8850_keypad_decoded key=0e",
).replace(
    "GSM service uplink sapi=0 pd=03 message=25",
    "8850_call_physical: action=end\n8850_keypad_decoded key=0f\n"
    "GSM service uplink sapi=0 pd=03 message=25",
).replace("data=040002000271012fc1", "data=041202000271012fc1").replace(
    "data=040000001117001a600000560000001400000001",
    "data=041202001117001a600000010000001400000001",
)


class Nokia8850OutgoingCallCheckTest(unittest.TestCase):
    def test_complete_signaling(self):
        verify(GOOD)

    def test_wrong_physical_key(self):
        with self.assertRaisesRegex(ValueError, "Send decode"):
            verify(GOOD.replace("key=0e", "key=11"))

    def test_wrong_number(self):
        with self.assertRaisesRegex(ValueError, "number"):
            verify(GOOD, "123")

    def test_missing_release_confirmation(self):
        with self.assertRaisesRegex(ValueError, "release confirmation"):
            verify(GOOD.replace(
                "RX enqueue type=89 payload=8 data=0000000000000000", ""))

    def test_blank_operator_frame(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            Image.new("L", (84, 48), 255).save(path / "8850_registered_idle.png")
            with self.assertRaisesRegex(ValueError, "operator presentation"):
                verify_frames(path)


if __name__ == "__main__":
    unittest.main()
