import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class DisplayPathTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.phone = (ROOT / "driver/nokia_3310.cpp").read_text()
        cls.gensio = (ROOT / "driver/nokia_gensio.cpp").read_text()

    def test_native_pcd8544_receives_msb_first_serial_bytes(self):
        self.assertIn('required_device<pcd8544_device> m_pcd8544', self.phone)
        self.assertIn('for (int bit = 7; bit >= 0; bit--)', self.gensio)
        self.assertIn('m_lcd_sdin_cb(BIT(data, bit))', self.gensio)
        self.assertNotIn('for (int i=7; i>=0; i--)', self.phone)

    def test_command_and_data_registers_drive_dc(self):
        self.assertIn('write_lcd(data, true)', self.gensio)
        self.assertIn('write_lcd(data, false)', self.gensio)
        self.assertIn('m_gensio->lcd_dc_cb().set(m_pcd8544', self.phone)


if __name__ == "__main__":
    unittest.main()
