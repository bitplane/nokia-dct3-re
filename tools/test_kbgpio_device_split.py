import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class KbgpioDeviceSplitTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.device = (ROOT / "driver/nokia_kbgpio.cpp").read_text()
        cls.header = (ROOT / "driver/nokia_kbgpio.h").read_text()
        cls.phone = (ROOT / "driver/nokia_dct3.cpp").read_text()

    def test_device_owns_matrix_and_irq_state(self):
        source = self.device + self.header
        for token in (
            "ROW_SIGNAL", "COLUMN_INPUT", "COLUMN_IRQ_MASK", "ROW_DIRECTION",
            "m_columns", "m_irq_latched", "sample_columns", "irq_acknowledge",
        ):
            self.assertIn(token, source)
        for token in ("m_keypad_columns", "m_keypad_irq_latched", "keypad_columns_r"):
            self.assertNotIn(token, self.phone)

    def test_phone_only_wires_inputs_configuration_and_irq(self):
        self.assertIn("required_device<nokia_kbgpio_device> m_kbgpio", self.phone)
        self.assertIn('matrix_cb(0).set_ioport("COL.0")', self.phone)
        self.assertIn('power_cb().set_ioport("PWR")', self.phone)
        self.assertIn("set_wiring_contract(product.keypad_wiring)", self.phone)
        self.assertIn("m_mad2->set_irq_line(KEYPAD_IRQ_LINE_NUM, state)", self.phone)

    def test_power_on_latch_distinguishes_cold_and_domain_reset(self):
        self.assertIn(
            "m_power_on = ~m_wiring.power_on_column_mask", self.device
        )
        self.assertIn("clear_power_on_latch", self.header)
        self.assertIn("m_kbgpio->clear_power_on_latch()", self.phone)

    def test_unknown_neighbor_registers_are_only_latches(self):
        self.assertIn("offset >= 0x28 && offset <= 0x2b", self.device)
        self.assertIn("offset >= 0x68 && offset <= 0x6b", self.device)
        self.assertIn("offset >= 0xa8 && offset <= 0xab", self.device)

    def test_device_is_save_state_safe_and_firmware_agnostic(self):
        source = self.device + self.header
        for token in ("save_item(NAME(m_regs))", "save_item(NAME(m_columns))", "save_item(NAME(m_irq_latched))"):
            self.assertIn(token, source)
        self.assertNotIn("pc()", source)
        self.assertNotRegex(source, r"0x2[0-9a-f]{5}")

    def test_row_topology_and_power_column_are_one_typed_contract(self):
        source = self.device + self.header + self.phone
        for token in (
            "struct wiring_contract",
            "m_wiring.rows == 5",
            "const unsigned row_count = m_wiring.rows",
            "if (!contract.valid())",
            "KEYPAD_NSE8",
            "KEYPAD_NHM5",
            "KEYPAD_NHM6",
            "KEYPAD_NHM2",
            "KEYPAD_NSE3",
        ):
            self.assertIn(token, source)
        for retired in (
            "set_five_rows",
            "set_power_on_column_mask",
            "keypad_five_rows",
        ):
            self.assertNotIn(retired, source)


if __name__ == "__main__":
    unittest.main()
