import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class DisplayPathTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.phone = (ROOT / "driver/nokia_3310.cpp").read_text()

    def test_native_pcd8544_receives_msb_first_serial_bytes(self):
        self.assertIn('required_device<pcd8544_device> m_pcd8544', self.phone)
        self.assertIn('for (int i=7; i>=0; i--)', self.phone)
        self.assertIn('m_pcd8544->sdin_w(BIT(data, i))', self.phone)

    def test_command_and_data_registers_drive_dc(self):
        self.assertIn('const bool lcd_data = !(offset & 0x40)', self.phone)
        self.assertIn('m_pcd8544->dc_w(lcd_data ? ASSERT_LINE : CLEAR_LINE)', self.phone)


if __name__ == "__main__":
    unittest.main()
