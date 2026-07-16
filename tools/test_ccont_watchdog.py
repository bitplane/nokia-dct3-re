import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class CcontWatchdogTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / "driver/nokia_ccont.cpp").read_text()

    def test_nonzero_register_value_is_the_reload(self):
        watchdog_case = self.source.split("case WATCHDOG:", 1)[1].split("break;", 1)[0]
        self.assertIn("if (data == 0x00)", watchdog_case)
        self.assertIn("m_power_cb(0)", watchdog_case)
        self.assertIn("m_watchdog = data", watchdog_case)

    def test_no_register_magic_disables_the_counter(self):
        watchdog_case = self.source.split("case WATCHDOG:", 1)[1].split("break;", 1)[0]
        self.assertNotIn("data == 0x20", watchdog_case)
        self.assertNotIn("data == 0x31", watchdog_case)
        self.assertNotIn("data == 0x3f", watchdog_case)
        self.assertNotIn("m_watchdog = 0", watchdog_case)


if __name__ == "__main__":
    unittest.main()
