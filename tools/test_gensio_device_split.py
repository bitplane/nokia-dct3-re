import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class GensioDeviceSplitTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.device = (ROOT / "driver/nokia_gensio.cpp").read_text()
        cls.header = (ROOT / "driver/nokia_gensio.h").read_text()
        cls.phone = (ROOT / "driver/nokia_dct3.cpp").read_text()

    def test_device_owns_transport_state(self):
        source = self.device + self.header
        for token in (
            "m_status", "STATUS_RX_READY", "ccont_rx_ready_on_write",
            "write_lcd", "m_ccont_read_cb", "m_ccont_write_cb",
        ):
            self.assertIn(token, source)
        self.assertNotIn("m_gensio_status", self.phone)
        self.assertNotIn("m_ccont->serial_r()", self.phone)
        self.assertNotIn("m_ccont->serial_w(data)", self.phone)

    def test_phone_delegates_sparse_window(self):
        self.assertIn("required_device<nokia_gensio_device> m_gensio", self.phone)
        self.assertIn("m_gensio->owns(offset)", self.phone)
        self.assertIn("m_gensio->read(offset)", self.phone)
        self.assertIn("m_gensio->write(offset, data)", self.phone)

    def test_select_banks_are_latches_not_guessed_peers(self):
        # Uninterpreted neighboring/select-bank offsets remain in the phone's
        # generic MAD2 backing store; GENSIO claims only product-wired ports.
        self.assertNotIn("offset >= 0xad", self.device)
        self.assertIn("m_mad2_regs[offset] = data", self.phone)
        source = (self.device + self.header).lower()
        for guess in ("rf synth", "audio codec", "cobba", "hagar"):
            self.assertNotIn(guess, source)

    def test_device_has_no_firmware_addresses_or_pc_checks(self):
        source = self.device + self.header
        self.assertNotIn("pc()", source)
        self.assertNotRegex(source, r"0x2[0-9a-f]{5}")


if __name__ == "__main__":
    unittest.main()
