import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class Mad2DeviceSplitTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.device = (ROOT / "driver/nokia_mad2.cpp").read_text()
        cls.header = (ROOT / "driver/nokia_mad2.h").read_text()
        cls.phone = (ROOT / "driver/nokia_dct3.cpp").read_text()

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

    def test_external_status_is_read_only_saved_input(self):
        source = self.device + self.header
        self.assertIn("set_external_status", self.header)
        self.assertIn("save_item(NAME(m_external_status))", self.device)
        self.assertIn("case 0x0e:", self.device)
        self.assertIn("return m_external_status;", self.device)
        self.assertIn("offset != 0x0c && offset != 0x0e", self.device)

    def test_timer1_wraps_at_terminal_count_and_interrupts_at_destination(self):
        callback = self.device.split("TIMER_CALLBACK_MEMBER(nokia_mad2_device::timer1_tick)", 1)[1]
        callback = callback.split("TIMER_CALLBACK_MEMBER(nokia_mad2_device::fiq8_tick)", 1)[0]
        self.assertIn("m_timer1_counter = (m_timer1_counter + 1) & m_timer1_destination;", callback)
        self.assertIn("m_timer1_counter == m_timer1_destination", callback)
        self.assertIn("assert_fiq(5);", callback)
        self.assertIn("m_mad2->set_timer1_hz(1'057);", self.phone)

    def test_timer1_destination_is_hardware_state_not_synthetic_time(self):
        self.assertIn("case 0x06: return m_timer1_destination >> 8;", self.device)
        self.assertIn("case 0x07: return m_timer1_destination;", self.device)
        self.assertIn("m_timer1_destination = 0x7fff", self.header)
        self.assertNotIn("m_timer1_counter + 0x40", self.device)

    def test_fiq8_is_the_centisecond_source(self):
        self.assertIn("m_fiq8_hz = 100;", self.header)
        self.assertIn("m_mad2->set_fiq8_hz(100);", self.phone)

    def test_clock_stop_is_saved_and_wakes_only_through_routed_interrupts(self):
        self.assertIn("save_item(NAME(m_sleeping))", self.device)
        self.assertIn("m_regs[offset] = data & ~0x02", self.device)
        self.assertIn('leave_sleep("FIQ")', self.device)
        self.assertIn('leave_sleep("IRQ")', self.device)
        self.assertIn("m_sleep_cb(m_sleeping ? 1 : 0)", self.device)

    def test_mad2_owns_the_proven_simi_clock_output(self):
        self.assertIn("auto simi_clock_cb()", self.header)
        self.assertIn("m_simi_clock_cb(BIT(m_regs[offset], 5))", self.device)
        self.assertIn("m_simi_clock_cb(BIT(m_regs[0x0d], 5))", self.device)
        self.assertIn("m_mad2->simi_clock_cb().set(m_simi", self.phone)

    def test_extended_irq_uses_cross_rom_status_and_ack_bits(self):
        self.assertIn("constexpr u8 EXT_IRQ_STATUS = 0x20;", self.device)
        self.assertIn("constexpr u8 EXT_IRQ_ACK = 0x40;", self.device)
        self.assertIn("(m_irq_status & LINE_EXTENDED) ? EXT_IRQ_STATUS : 0", self.device)
        self.assertIn("if (data == EXT_IRQ_ACK)", self.device)
        self.assertIn("ack_irq(LINE_EXTENDED);", self.device)
        self.assertNotIn("EXT_IRQ_MASK", self.device)

    def test_reset_control_requests_the_board_reset_domain(self):
        self.assertIn("auto reset_cb()", self.header)
        self.assertIn("if (BIT(data, 2) && !BIT(old, 2))", self.device)
        self.assertIn("m_reset_cb(1);", self.device)
        self.assertIn("m_mad2->reset_cb().set", self.phone)
        watchdog = self.phone.split("// MAD2 watchdog", 1)[1].split("// Hardware RAM", 1)[0]
        self.assertIn("reset_digital_baseband();", watchdog)
        self.assertIn("set_reset_cause(0x02)", watchdog)

    def test_dsp_reset_readback_uses_one_typed_wiring_contract(self):
        source = self.device + self.header
        for token in (
            "struct dsp_reset_wiring_contract",
            "m_dsp_reset_wiring.enabled()",
            "m_dsp_reset_wiring.release_mask",
            "m_dsp_reset_wiring.running_status",
            "if (!contract.valid())",
            "set_dsp_reset_wiring_contract(product.dsp_reset_wiring)",
            "DSP_RESET_WIRING_3410",
        ):
            self.assertIn(token, source + self.phone)
        for retired in (
            "set_dsp_reset_running_status",
            "set_dsp_release_mask",
            "dsp_reset_running_status",
            "dsp_release_mask",
        ):
            self.assertNotIn(retired, source + self.phone)


if __name__ == "__main__":
    unittest.main()
