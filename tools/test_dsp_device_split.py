import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class DspDeviceSplitTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.transport = (ROOT / "driver/nokia_dspif.cpp").read_text() + (ROOT / "driver/nokia_dspif.h").read_text()
        cls.hle = (ROOT / "driver/nokia_dsp_hle.cpp").read_text() + (ROOT / "driver/nokia_dsp_hle.h").read_text()
        cls.external = (ROOT / "driver/nokia_external_service.cpp").read_text() + (ROOT / "driver/nokia_external_service.h").read_text()
        cls.network = (ROOT / "driver/nokia_gsm_network.cpp").read_text() + (ROOT / "driver/nokia_gsm_network.h").read_text()
        cls.radio = (ROOT / "driver/nokia_radio_peer.cpp").read_text() + (ROOT / "driver/nokia_radio_peer.h").read_text()
        cls.phone = (ROOT / "driver/nokia_dct3.cpp").read_text()

    def test_transport_has_no_service_protocol(self):
        for token in ("DISCOVERY_NODE", "registration_sent", "channel_map", "0x64, 0x01"):
            self.assertNotIn(token, self.transport)

    def test_external_peer_has_no_hardware_ownership(self):
        for token in ("shared_r", "shared_w", "dspif", "fiq0", "service_irq", "TX_PRODUCER"):
            self.assertNotIn(token, self.external)

    def test_hle_has_no_session_state(self):
        for token in ("registration_sent", "channel_map_sent", "DISCOVERY_NODE"):
            self.assertNotIn(token, self.hle)

    def test_bootstrap_is_peer_publication_not_read_overlay(self):
        self.assertIn("peer_shared_w", self.transport)
        self.assertIn("publish_bootstrap_state", self.hle)
        self.assertNotIn("bootstrap_r", self.hle + self.phone)
        self.assertIn("m_bootstrap_exchange_limit", self.hle)
        self.assertNotIn("m_bootstrap_exchange_count == 64", self.hle)

    def test_transport_has_no_dsp_bootstrap_behavior_configuration(self):
        for token in (
            "m_bootstrap_ping_pong", "m_code_block_request",
            "m_parked_boot_status", "m_boot_status_response",
        ):
            self.assertNotIn(token, self.transport)
            self.assertIn(token, self.hle)
        self.assertIn("shared_002_write_cb", self.transport)
        self.assertIn("shared_0fe_read_cb", self.transport)
        self.assertIn("shared_100_write_cb", self.transport)
        self.assertIn("peer_shared_w(0x002 / 2, m_boot_status_response)", self.hle)

    def test_external_peer_uses_acknowledged_startup_phases(self):
        self.assertIn("m_registration_acknowledged && !m_channel_map_sent", self.external)
        self.assertIn("m_channel_map_acknowledged && !m_empty_ack_sent", self.external)
        self.assertNotIn("queue_service_frame(0x64, 0x05", self.external)

    def test_phone_composes_peer_devices(self):
        for token in (
            "required_device<nokia_dspif_device> m_dspif",
            "required_device<nokia_dsp_hle_device> m_dsp_hle",
            "required_device<nokia_external_service_peer_device> m_external_service_peer",
            "required_device<nokia_gsm_network_device> m_gsm_network",
            "required_device<nokia_radio_peer_device> m_radio_peer",
        ):
            self.assertIn(token, self.phone)

    def test_gsm_system_information_is_outside_nokia_transport_model(self):
        self.assertIn("SYSTEM_INFORMATION", self.network)
        self.assertIn("0x00, 0xf1, 0x10", self.network)
        self.assertNotIn("0x49, 0x06, 0x1b", self.hle)
        self.assertIn("m_gsm_network->system_information", self.radio)

    def test_radio_transaction_state_is_outside_bootstrap_hle(self):
        for token in ("phase::", "candidate_sync", "location_update_accept", "system_information"):
            self.assertNotIn(token, self.hle)
        for token in ("receive_packet", "next_report_type", "advance_after_report", "m_reports_remaining"):
            self.assertIn(token, self.radio)

    def test_network_has_no_nokia_transport_ownership(self):
        for token in ("dspif", "enqueue_rx_packet", "CHANNEL_CHANGED_CNF", "m_reports_remaining"):
            self.assertNotIn(token, self.network)


if __name__ == "__main__":
    unittest.main()
