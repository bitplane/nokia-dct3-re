import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class Mad2DeviceSplitTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.device = (ROOT / "driver/nokia_mad2.cpp").read_text()
        cls.header = (ROOT / "driver/nokia_mad2.h").read_text()
        cls.phone = (ROOT / "driver/nokia_3310.cpp").read_text()

    def test_device_owns_ctsi_state(self):
        source = self.device + self.header
        for token in (
            "m_fiq_status", "m_irq_status", "m_timer0_counter",
            "m_timer1_counter", "update_fiq_line", "update_irq_line",
            "watchdog_tick",
        ):
            self.assertIn(token, source)
            self.assertNotIn(token + " =", self.phone)

    def test_phone_delegates_ctsi_window(self):
        self.assertIn("required_device<nokia_mad2_device> m_mad2", self.phone)
        self.assertIn("offset <= MAD2_FIQ8_CTRL", self.phone)
        self.assertIn("m_mad2->read(offset)", self.phone)
        self.assertIn("m_mad2->write(offset, data)", self.phone)

    def test_device_has_no_firmware_addresses(self):
        source = self.device + self.header
        self.assertNotIn("pc()", source)
        self.assertNotRegex(source, r"0x2[0-9a-f]{5}")

    def test_register_storage_covers_masked_window(self):
        self.assertIn("m_regs[0x20]", self.header)
        self.assertIn("offset & 0x1f", self.header)

    def test_timer1_is_free_running_and_interrupts_on_wrap(self):
        callback = self.device.split("TIMER_CALLBACK_MEMBER(nokia_mad2_device::timer1_tick)", 1)[1]
        callback = callback.split("TIMER_CALLBACK_MEMBER(nokia_mad2_device::fiq8_tick)", 1)[0]
        self.assertIn("m_timer1_counter++;", callback)
        self.assertIn("if (m_timer1_counter == 0)", callback)
        self.assertIn("assert_fiq(5);", callback)
        self.assertNotIn("m_timer1_counter = 0;", callback)
        self.assertIn("m_mad2->set_timer1_hz(33'055);", self.phone)

    def test_timer1_destination_is_a_latch_not_synthetic_time(self):
        self.assertIn("case 0x06: return m_regs[offset];", self.device)
        self.assertIn("case 0x07: return m_regs[offset];", self.device)
        self.assertNotIn("m_timer1_counter + 0x40", self.device)


if __name__ == "__main__":
    unittest.main()
