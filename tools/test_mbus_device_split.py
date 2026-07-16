import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class MbusDeviceSplitTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.device = (ROOT / "driver/nokia_mbus.cpp").read_text()
        cls.header = (ROOT / "driver/nokia_mbus.h").read_text()
        cls.phone = (ROOT / "driver/nokia_3310.cpp").read_text()

    def test_device_owns_controller_state(self):
        source = self.device + self.header
        for token in ("m_control", "m_status_latch", "m_rx_ready", "m_tx_pending", "receive_byte"):
            self.assertIn(token, source)
        for token in ("schedule_mbus_fiq", "signal_mbus_fiq", "complete_mbus_transfer", "m_timer_mbus;"):
            self.assertNotIn(token, self.phone)

    def test_phone_delegates_register_window(self):
        self.assertIn("required_device<nokia_mbus_device> m_mbus", self.phone)
        self.assertIn("m_mbus->read(offset - MAD2_MBUS_CTRL)", self.phone)
        self.assertIn("m_mbus->write(offset - MAD2_MBUS_CTRL, data)", self.phone)

    def test_device_has_no_firmware_addresses_or_peer_replies(self):
        source = self.device + self.header
        self.assertNotIn("pc()", source)
        self.assertNotRegex(source, r"0x2[0-9a-f]{5}")
        self.assertNotIn("service", source.lower())

    def test_character_timing_is_physical_not_an_environment_knob(self):
        self.assertIn("attotime::from_hz(960)", self.header)
        self.assertNotIn("MBUS_BYTE_DELAY_MS", self.phone)
        self.assertNotIn("set_byte_delay", self.header)


if __name__ == "__main__":
    unittest.main()
