import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class PupDeviceSplitTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.device = (ROOT / "driver/nokia_pup.cpp").read_text()
        cls.header = (ROOT / "driver/nokia_pup.h").read_text()
        cls.phone = (ROOT / "driver/nokia_dct3.cpp").read_text()

    def test_device_owns_output_and_genio_registers(self):
        source = self.device + self.header
        for token in (
            "CONTROL", "VIBRATOR_CONTROL", "BUZZER_DIVIDER_MSB",
            "BUZZER_VOLUME", "GENIO_SIGNAL", "GENIO_DIRECTION",
            "update_buzzer", "update_vibrator", "update_genio",
        ):
            self.assertIn(token, source)
        for token in ("update_buzzer", "update_vibrator", "MAD2_BUZZER_DIVIDER_MSB"):
            self.assertNotIn(token, self.phone)

    def test_genio_retains_open_drain_eeprom_contract(self):
        self.assertIn("BIT(direction, 0) ? BIT(signal, 0) : 1", self.device)
        self.assertIn("BIT(m_regs[GENIO_DIRECTION], 0)", self.device)
        self.assertIn("m_eeprom_sda_read_cb", self.device + self.header)
        self.assertIn("eeprom_sda_read_cb().set(m_eeprom", self.phone)

    def test_phone_only_wires_external_outputs(self):
        self.assertIn("required_device<nokia_pup_device> m_pup", self.phone)
        self.assertIn("buzzer_clock_cb().set", self.phone)
        self.assertIn("buzzer_enable_cb().set", self.phone)
        self.assertIn("vibrator_enable_cb().set", self.phone)

    def test_device_restores_external_lines_after_load(self):
        self.assertIn("save_item(NAME(m_regs))", self.device)
        self.assertIn("register_postload", self.device)
        for token in ("update_genio();", "update_buzzer();", "update_vibrator();"):
            self.assertIn(token, self.device)

    def test_unknown_genio_latch_has_no_side_effect(self):
        self.assertIn("GENIO_LATCH = 0x22", self.device)
        write = self.device.split("void nokia_pup_device::write", 1)[1]
        self.assertNotIn("GENIO_LATCH]", write)

    def test_device_has_no_firmware_addresses(self):
        source = self.device + self.header
        self.assertNotIn("pc()", source)
        self.assertNotRegex(source, r"0x2[0-9a-f]{5}")


if __name__ == "__main__":
    unittest.main()
