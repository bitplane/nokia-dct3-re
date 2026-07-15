import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class SimDeviceSplitTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.card = (ROOT / "driver/nokia_sim_card.cpp").read_text()
        cls.card_header = (ROOT / "driver/nokia_sim_card.h").read_text()
        cls.simi = (ROOT / "driver/nokia_simi.cpp").read_text()
        cls.simi_header = (ROOT / "driver/nokia_simi.h").read_text()
        cls.phone = (ROOT / "driver/nokia_3310.cpp").read_text()

    def test_card_has_no_mad2_controller_state(self):
        for token in (
            "m_iir", "m_rx_timer", "m_uart_tx_fifo", "tx_fifo_control_w",
            "rx_fifo_control_w", "attotime", "irq_cb",
        ):
            self.assertNotIn(token, self.card + self.card_header)

    def test_simi_has_no_card_protocol_or_profile_state(self):
        for token in (
            "m_selected_file", "m_selected_df", "finish_header", "queue_fcp",
            "ef_byte", "m_cphs_aoc",
        ):
            self.assertNotIn(token, self.simi + self.simi_header)

    def test_phone_routes_registers_and_fiq_through_simi(self):
        self.assertIn("required_device<nokia_simi_device> m_simi", self.phone)
        self.assertIn("m_simi->irq_cb().set", self.phone)
        self.assertIn("m_sim_card->response_cb().set(m_simi", self.phone)
        for method in (
            "rxd_r", "iir_r", "control_r", "rx_count_r", "tx_count_r",
            "txd_w", "iir_w", "control_w", "rx_fifo_control_w",
            "tx_fifo_control_w",
        ):
            self.assertIn(f"m_simi->{method}", self.phone)


if __name__ == "__main__":
    unittest.main()
