import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class DisplayPathTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.phone = (ROOT / "driver/nokia_dct3.cpp").read_text()
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
        for declaration in (
            'struct display_geometry_contract',
            'constexpr bool valid() const',
            'constexpr display_geometry_contract DISPLAY_3410',
            '102, 72, 96, 65',
            'result.display = DISPLAY_3410;',
            'm_lcd->set_geometry(product.display.controller_width',
            'screen->set_size(product.display.visible_width',
        ):
            self.assertIn(declaration, self.phone)
        self.assertIn('visible_width <= controller_width', self.phone)
        self.assertIn('visible_height <= controller_height', self.phone)
        self.assertIn('if (!product.display.valid())', self.phone)
        for retired in (
            'lcd_controller_width',
            'lcd_controller_height',
            'lcd_visible_width',
            'lcd_visible_height',
        ):
            self.assertNotIn(retired, self.phone)

    def test_unvalidated_products_do_not_bypass_the_geometry_contract(self):
        for machine, next_machine in (
            ('noki7110', 'noki6210'),
            ('noki6210', 'noki3410'),
        ):
            body = self.phone.split(
                f'void nokia_dct3_state::{machine}(machine_config &config)', 1
            )[1].split(
                f'void nokia_dct3_state::{next_machine}(machine_config &config)',
                1,
            )[0]
            self.assertNotIn('set_size', body)
            self.assertNotIn('set_visarea', body)

    def test_default_y_command_mask_is_unchanged(self):
        self.assertIn('m_controller_banks > 8 ? 0x0f : 0x07', self.lcd_patch)
        self.assertNotIn('m_addr_y = (cmd & 0x0f) % m_controller_banks', self.lcd_patch)


if __name__ == "__main__":
    unittest.main()
