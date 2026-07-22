import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class DisplayPathTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.phone = (ROOT / "driver/nokia_3310.cpp").read_text()
        cls.gensio = (ROOT / "driver/nokia_gensio.cpp").read_text()
        cls.lcd_patch = (ROOT / "patches/mame-pcd8544-geometry.patch").read_text()

    def test_native_pcd8544_receives_msb_first_serial_bytes(self):
        self.assertIn('required_device<pcd8544_device> m_lcd', self.phone)
        self.assertIn('PCD8544(config, m_lcd)', self.phone)
        self.assertIn('for (int bit = 7; bit >= 0; bit--)', self.gensio)
        self.assertIn('m_lcd_sdin_cb(BIT(data, bit))', self.gensio)
        self.assertIn('void pcd8544_device::set_geometry', self.lcd_patch)
        self.assertNotIn('for (int i=7; i>=0; i--)', self.phone)
        self.assertFalse((ROOT / "driver/nokia_lcd.cpp").exists())
        self.assertFalse((ROOT / "driver/nokia_lcd.h").exists())

    def test_command_and_data_registers_drive_dc(self):
        self.assertIn('write_lcd(data, true)', self.gensio)
        self.assertIn('write_lcd(data, false)', self.gensio)
        self.assertIn('m_gensio->lcd_dc_cb().set(m_lcd', self.phone)

    def test_controller_and_visible_geometry_are_separate(self):
        self.assertIn('static constexpr unsigned MAX_WIDTH = 102', self.lcd_patch)
        self.assertIn('static constexpr unsigned MAX_BANKS = 9', self.lcd_patch)
        self.assertIn('m_addr_y * m_controller_width + m_addr_x', self.lcd_patch)
        self.assertIn('m_visible_height', self.lcd_patch)
        self.assertIn('m_controller_width(84)', self.lcd_patch)
        self.assertIn('m_visible_height(48)', self.lcd_patch)
        self.assertIn('102, 72, 96, 65, ADC_3310', self.phone)


if __name__ == "__main__":
    unittest.main()
