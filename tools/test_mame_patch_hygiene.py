import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class MamePatchHygieneTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.flash = (ROOT / "mame/src/devices/machine/intelfsh.cpp").read_text()
        cls.flash_h = (ROOT / "mame/src/devices/machine/intelfsh.h").read_text()
        cls.lcd = (ROOT / "mame/src/devices/video/pcd8544.cpp").read_text()

    def test_flash_behavior_uses_part_attributes(self):
        self.assertIn("m_device_id_address", self.flash_h)
        self.assertIn("m_main_block_size", self.flash_h)
        self.assertIn("m_parameter_block_size", self.flash_h)
        self.assertIn("m_parameter_block_count", self.flash_h)
        self.assertNotIn("if (m_device_id == 0x88ba)", self.flash)
        self.assertNotIn("m_device_id == 0x8896", self.flash)

    def test_nor_programming_only_clears_bits(self):
        body = self.flash.split("case FM_WRITEPART1:", 1)[1]
        body = body.split("m_status = 0x80;", 1)[0]
        self.assertIn("m_data[address] &= data", body)
        self.assertIn("m_data[address*2] &= data >> 8", body)
        self.assertNotIn("m_data[address] = data", body)

    def test_parameter_erase_is_geometry_driven(self):
        self.assertIn("m_parameter_block_size * m_parameter_block_count", self.flash)
        self.assertIn("m_top_boot_sector && byte_address", self.flash)
        self.assertIn("m_bot_boot_sector && byte_address", self.flash)

    def test_lcd_defaults_and_command_mask_are_preserved(self):
        self.assertIn("m_controller_width(84)", self.lcd)
        self.assertIn("m_controller_banks(6)", self.lcd)
        self.assertIn("m_visible_width(84)", self.lcd)
        self.assertIn("m_visible_height(48)", self.lcd)
        self.assertIn("m_controller_banks > 8 ? 0x0f : 0x07", self.lcd)


if __name__ == "__main__":
    unittest.main()
