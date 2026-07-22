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

    def test_timer1_is_15_bit_and_interrupts_at_destination(self):
        callback = self.device.split("TIMER_CALLBACK_MEMBER(nokia_mad2_device::timer1_tick)", 1)[1]
        callback = callback.split("TIMER_CALLBACK_MEMBER(nokia_mad2_device::fiq8_tick)", 1)[0]
        self.assertIn("(m_timer1_counter + 1) & 0x7fff", callback)
        self.assertIn("m_timer1_counter == m_timer1_destination", callback)
        self.assertIn("assert_fiq(5);", callback)
        self.assertIn("NOKI3210_TIMER1_HZ\", 1'057", self.phone)

    def test_timer1_destination_is_hardware_state_not_synthetic_time(self):
        self.assertIn("case 0x06: return m_timer1_destination >> 8;", self.device)
        self.assertIn("case 0x07: return m_timer1_destination;", self.device)
        self.assertIn("m_timer1_destination = 0x7fff", self.header)
        self.assertNotIn("m_timer1_counter + 0x40", self.device)

    def test_clock_stop_is_saved_and_wakes_only_through_routed_interrupts(self):
        self.assertIn("save_item(NAME(m_sleeping))", self.device)
        self.assertIn("m_regs[offset] = data & ~0x02", self.device)
        self.assertIn('leave_sleep("FIQ")', self.device)
        self.assertIn('leave_sleep("IRQ")', self.device)
        self.assertIn("m_sleep_cb(m_sleeping ? 1 : 0)", self.device)

    def test_reset_control_requests_the_board_reset_domain(self):
        self.assertIn("auto reset_cb()", self.header)
        self.assertIn("if (BIT(data, 2) && !BIT(old, 2))", self.device)
        self.assertIn("m_reset_cb(1);", self.device)
        self.assertIn("m_mad2->reset_cb().set", self.phone)
        watchdog = self.phone.split("// MAD2 watchdog", 1)[1].split("// Hardware RAM", 1)[0]
        self.assertIn("reset_digital_baseband();", watchdog)
        self.assertIn("set_reset_cause(0x02)", watchdog)


if __name__ == "__main__":
    unittest.main()
