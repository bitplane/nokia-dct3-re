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

    def test_wddisx_is_a_device_input(self):
        self.assertIn("m_wddisx_grounded || m_watchdog == 0", self.source)
        phone = (ROOT / "driver/nokia_dct3.cpp").read_text()
        self.assertIn("set_wddisx_grounded", phone)
        self.assertNotIn("NOKIA_DCT3_DISABLE_CCONT_WATCHDOG", phone)
        product = phone.split(
            "constexpr nokia_product_config make_3210_config()", 1
        )[1].split("constexpr nokia_product_config make_3310_config()", 1)[0]
        self.assertIn("result.power_on_column_mask = 0x01;", product)
        self.assertNotIn("result.ccont_wddisx_grounded = true;", product)

    def test_expiry_resets_the_complete_digital_baseband_domain(self):
        phone = (ROOT / "driver/nokia_dct3.cpp").read_text()
        timer = phone.split(
            "TIMER_CALLBACK_MEMBER(nokia_dct3_state::timer_watchdog)", 1
        )[1].split("// MAD2 watchdog", 1)[0]
        self.assertIn("reset_digital_baseband();", timer)
        self.assertNotIn("m_maincpu->reset();", timer)
        self.assertNotIn("m_mad2->reset();", timer)

    def test_rtc_is_internal_and_deterministic(self):
        self.assertNotIn("current_datetime", self.source)
        self.assertIn("m_regs[RTC_HOUR] = 12", self.source)
        self.assertIn("attotime::from_seconds(1)", self.source)
        self.assertIn("++m_regs[RTC_SECOND] >= 60", self.source)
        self.assertIn("RTC_SECOND = 0x07", self.source)
        self.assertIn("RTC_DAY = 0x0a", self.source)
        self.assertIn("Reading the selected register completes", self.source)
        self.assertIn("if (m_regs[RTC_SECOND] != 0)", self.source)

    def test_rtc_sources_use_recovered_status_bits(self):
        self.assertIn("IRQ_RTC_SECOND = 0x10", self.source)
        self.assertIn("IRQ_RTC_MINUTE = 0x20", self.source)

        self.assertIn("IRQ_RTC_ALARM = 0x80", self.source)
        self.assertIn("m_rtc_alarm_armed", self.source)
        self.assertIn("case RTC_ALARM_MINUTE:", self.source)


if __name__ == "__main__":
    unittest.main()
