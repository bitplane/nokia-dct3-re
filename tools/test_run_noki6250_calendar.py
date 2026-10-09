import tempfile
from pathlib import Path
import unittest

from PIL import Image
from tools.run_noki6250_calendar import check_frame, check_phase


class CalendarCheckTest(unittest.TestCase):
    def trace(self, cold=False):
        actions = ["menu", *[f"down_{i}" for i in range(1, 8)], "calendar"]
        if not cold:
            actions += [*[f"time_{i}" for i in range(1, 5)], "time_confirm",
                        *[f"date_{i}" for i in range(1, 9)], "date_confirm"]
        text = "".join(f"6250_calendar_physical: action={action}\n" for action in actions)
        if cold:
            text += "ccont_rtc: event=read reg=09 data=0d t=2\n"
            text += "ccont_rtc: event=read reg=08 data=2f t=2\n"
        else:
            text = text.replace("action=time_confirm\n", "action=time_confirm\n"
                                "ccont_rtc: event=alarm_write reg=0b data=2f t=42\n"
                                "ccont_rtc: event=alarm_write reg=0c data=8d t=42\n")
        text += "6250_calendar_physical: event=" + ("cold_presented" if cold else "entered_presented") + "\n"
        return text

    def test_both_phase_contracts(self):
        check_phase(self.trace(), False)
        check_phase(self.trace(True), True)

    def test_cold_replacement_input_rejected(self):
        with self.assertRaisesRegex(ValueError, "sequence"):
            check_phase(self.trace(True) + "6250_calendar_physical: action=date_1\n", True)

    def test_wrong_clock_and_missing_programming_rejected(self):
        for cold in (False, True):
            with self.subTest(cold=cold), self.assertRaises(ValueError):
                check_phase(self.trace(cold).replace("data=2f", "data=00"), cold)

    def test_missing_duplicate_and_lua_error_rejected(self):
        for text in (self.trace().replace("action=down_3", "action=down_4"),
                     self.trace() + "6250_calendar_physical: event=entered_presented\n",
                     self.trace() + "[LUA ERROR]"):
            with self.assertRaises(ValueError):
                check_phase(text, False)

    def test_frame_rejects_wrong_geometry_and_pixels(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "frame.png"
            for size in ((84, 48), (96, 60)):
                Image.new("L", size).save(path)
                with self.assertRaises(ValueError):
                    check_frame(path, "0" * 64)


if __name__ == "__main__":
    unittest.main()
