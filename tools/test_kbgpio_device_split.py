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
            "row_signal", "column_input", "column_irq_mask", "row_direction",
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

    def test_column_irq_is_masked_and_edge_symmetric(self):
        self.assertIn(
            "~m_regs[m_wiring.column_irq_mask] & 0x1f",
            self.device,
        )
        self.assertNotIn("physical_edge", self.device + self.header)

    def test_register_locations_are_product_wiring(self):
        for field in ("row_signal", "column_input", "column_irq_mask", "row_direction"):
            self.assertIn(f"offset == m_wiring.{field}", self.device)
        self.assertIn("KEYPAD_NSE1", self.phone)
        self.assertIn("5, 0x02, 0x31, 0x30, 0x33, 0x2f", self.phone)

    def test_device_is_save_state_safe_and_firmware_agnostic(self):
        source = self.device + self.header
        for token in ("save_item(NAME(m_regs))", "save_item(NAME(m_columns))", "save_item(NAME(m_irq_latched))", "save_item(NAME(m_power_on))"):
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
            "KEYPAD_NSE1",
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
