import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class B3FlashDeviceSplitTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.phone = (ROOT / "driver/nokia_3310.cpp").read_text()
        cls.device = (ROOT / "driver/nokia_b3_flash.cpp").read_text()

    def test_phone_only_routes_and_configures_b3_flash(self):
        self.assertIn("return m_b3_flash->read(offset, mem_mask);", self.phone)
        self.assertIn("m_b3_flash->write(offset, data, mem_mask);", self.phone)
        self.assertIn("m_b3_flash->set_enabled(product.flash_b3_block_lock);", self.phone)
        for state in (
            "m_flash_b3_lock_command",
            "m_flash_b3_erase_active",
            "m_flash_b3_erase_suspended",
            "m_timer_flash_b3_erase",
        ):
            self.assertNotIn(state, self.phone)

    def test_adapter_owns_mutable_protocol_state_and_save_state(self):
        for state in (
            "m_lock_command",
            "m_program_data",
            "m_erase_confirm",
            "m_erase_active",
            "m_erase_suspended",
            "m_status_override",
            "m_erase_remaining_us",
        ):
            self.assertIn(f"save_item(NAME({state}))", self.device)
        self.assertIn("timer_alloc(FUNC(nokia_b3_flash_device::erase_complete)", self.device)

    def test_adapter_contains_no_firmware_address_or_cpu_state(self):
        self.assertNotIn("0x002", self.device)
        self.assertNotIn("m_maincpu", self.device)
        self.assertNotIn("set_state_int", self.device)


if __name__ == "__main__":
    unittest.main()
