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
        cls.mad2 = (ROOT / "driver/nokia_mad2.cpp").read_text()
        cls.phone = (ROOT / "driver/nokia_dct3.cpp").read_text()

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

    def test_controller_presence_is_separate_from_synthetic_card(self):
        self.assertIn("void set_card_present(bool present)", self.simi_header)
        self.assertIn("if (m_card_present)", self.simi)
        self.assertIn(
            "m_simi->set_enabled(m_product.simi_controller &&", self.phone
        )
        self.assertIn(
            "m_simi->set_card_present(m_product.synthetic_sim_card &&", self.phone
        )

    def test_character_timing_matches_observed_default_pps(self):
        self.assertIn("TA1=0x05", self.simi)
        self.assertIn("attotime::from_hz(960)", self.simi)
        self.assertNotIn("attotime::from_hz(15'360)", self.simi)
        self.assertIn("m_rx_ready && m_rx_count != 0 ? 1 : 0", self.simi)
        self.assertNotIn("from_usec(10)", self.simi + self.simi_header)
        self.assertNotIn("from_usec(100)", self.simi + self.simi_header)

    def test_mad2_clock_gate_freezes_transport_without_erasing_state(self):
        self.assertIn("void set_clock_enabled(int state)", self.simi_header)
        gate = self.simi.split("void nokia_simi_device::set_clock_enabled", 1)[1]
        gate = gate.split("u8 nokia_simi_device::control_r", 1)[0]
        self.assertIn("m_rx_timer->adjust(attotime::never)", gate)
        self.assertNotIn("m_rx_head =", gate)
        self.assertIn("m_mad2->simi_clock_cb().set(m_simi", self.phone)
        self.assertIn("m_simi_clock_cb(BIT(m_regs[offset], 5))", self.mad2)

    def test_card_owns_persistent_linear_fixed_adn(self):
        self.assertIn("public device_nvram_interface", self.card_header)
        self.assertIn("{ 0x6f3a, 0x7f10, 50 * 32, 32, file_structure::linear_fixed, true }", self.card)
        self.assertIn("Services 2 (ADN) and 4 (SMS) allocated", self.card)
        self.assertIn("fcp[8] = 0x01", self.card)
        self.assertIn("void nokia_sim_card_device::update_record()", self.card)
        self.assertIn("save_item(NAME(m_adn))", self.card)

    def test_registration_files_form_a_coherent_phase2_profile(self):
        self.assertIn("{ 0x6fad, 0x7f20, 4, 0", self.card)
        self.assertIn("administrative_data[] = { 0x00, 0xff, 0xff, 0x02 }", self.card)
        self.assertIn("plmn_selector[] = { 0x00, 0xf1, 0x10 }", self.card)
        self.assertIn("{ 0x6f20, 0x7f20, 9, 0, file_structure::transparent, true }", self.card)
        self.assertIn("{ 0x6f74, 0x7f20, 16, 0, file_structure::transparent, true }", self.card)
        self.assertIn("{ 0x6f7e, 0x7f20, 11, 0, file_structure::transparent, true }", self.card)
        self.assertIn("void nokia_sim_card_device::update_binary()", self.card)
        self.assertIn("save_item(NAME(m_loci))", self.card)
        self.assertIn("save_item(NAME(m_kc))", self.card)
        self.assertIn("save_item(NAME(m_bcch))", self.card)
        self.assertIn("m_loci[10] = 0x01", self.card)
        self.assertIn("if (m_cached_location)", self.card)

    def test_sms_files_are_declared_and_persistent(self):
        self.assertIn(
            "{ 0x6f3c, 0x7f10, sms_record_count * sms_record_length",
            self.card)
        self.assertIn(
            "{ 0x6f42, 0x7f10, smsp_record_count * smsp_record_length",
            self.card)
        self.assertIn("sms_record_length = 176", self.card_header)
        self.assertIn("Services 2 (ADN) and 4 (SMS)", self.card)
        self.assertIn("Service 12: SMS parameters", self.card)
        self.assertIn("save_item(NAME(m_sms))", self.card)
        self.assertIn("save_item(NAME(m_smsp))", self.card)
        self.assertIn("case 0x6f3c: return m_sms", self.card)
        self.assertIn("case 0x6f42: return m_smsp", self.card)


if __name__ == "__main__":
    unittest.main()
