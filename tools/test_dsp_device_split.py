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
        cls.session = (ROOT / "driver/nokia_gsm_session.cpp").read_text() + (ROOT / "driver/nokia_gsm_session.h").read_text()
        cls.lapdm = (ROOT / "driver/nokia_lapdm_link.cpp").read_text() + (ROOT / "driver/nokia_lapdm_link.h").read_text()
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

    def test_dsp_hle_decodes_product_configured_control_field(self):
        self.assertIn("mcu_control_word_cb", self.hle)
        self.assertIn("shared_word(0x0a8 / 2)", self.hle)
        self.assertNotIn("mcu_control_word", self.transport)
        self.assertNotIn("mcu_control_word", self.network + self.session)
        for token in (
            "m_mcu_control_wire >> 12",
            "m_mcu_control_wire & 0x0fff",
            "m_parameter_command",
            "m_speech_request_mask",
            "m_speech_request_value",
        ):
            self.assertIn(token, self.hle)
        # The paired-ROM evidence supports a masked field transition, not an
        # NSE-8 whole-word state machine embedded in the generic DSP device.
        self.assertNotIn("m_mcu_control_word == 0x060b", self.hle)

    def test_dsp_hle_owns_dsp_addressed_memory_upload(self):
        for token in (
            "consume_memory_upload", "packet.type != 0x51",
            "m_data_memory[address]", "m_data_memory_loaded[address]",
            "data_word(u16 address)", "data_word_loaded(u16 address)",
        ):
            self.assertIn(token, self.hle)
        self.assertNotIn("m_data_memory", self.transport)
        self.assertNotIn("m_data_memory", self.network + self.session)

    def test_service_control_completion_framing_is_product_typed(self):
        for token in (
            "struct service_control_contract",
            "DSP_SERVICE_CONTROL_COMPACT",
            "DSP_SERVICE_CONTROL_FRAMED",
            "0x00, 0x04, 0x01, 0x00, 0x0d, 0x00",
        ):
            self.assertIn(token, self.hle + self.phone)
        self.assertIn(
            "set_service_control_contract(product.dsp_service_control)",
            self.phone,
        )
        self.assertNotIn("service_control_contract", self.transport)
        self.assertNotIn("service_control_profile", self.hle + self.phone)

    def test_reusable_service_devices_have_no_product_named_profiles(self):
        for token in ("nse8", "nhm5"):
            self.assertNotIn(token, (self.hle + self.external).lower())
        for token in ("application_profile", "service_control_profile"):
            self.assertNotIn(token, self.hle + self.external)

    def test_bootstrap_is_peer_publication_not_read_overlay(self):
        self.assertIn("peer_shared_w", self.transport)
        self.assertIn("publish_bootstrap_state", self.hle)
        self.assertNotIn("bootstrap_r", self.hle + self.phone)
        self.assertIn("m_bootstrap_exchange_limit", self.hle)
        self.assertNotIn("m_bootstrap_exchange_count == 64", self.hle)

    def test_transport_has_no_dsp_bootstrap_behavior_configuration(self):
        for token in (
            "m_bootstrap_completion", "m_bootstrap_preupload",
            "m_bootstrap_ping_pong", "m_code_block_request",
            "m_parked_boot_status", "m_boot_status_response",
        ):
            self.assertNotIn(token, self.transport)
            self.assertIn(token, self.hle)
        self.assertIn("shared_002_write_cb", self.transport)
        self.assertIn("shared_006_write_cb", self.transport)
        self.assertIn("shared_0fe_read_cb", self.transport)
        self.assertIn("shared_100_write_cb", self.transport)
        self.assertIn("peer_shared_w(0x002 / 2, m_boot_status_response)", self.hle)
        self.assertIn(
            "bootstrap_completion_profile::ready_words_one",
            self.hle,
        )
        self.assertIn(
            "bootstrap_completion_profile::"
            "nse3_flash_verification_b06_verdict_unknown",
            self.hle + self.phone,
        )
        self.assertIn("peer_shared_w(0x000 / 2, 0x0b06)", self.hle)
        self.assertNotIn("peer_shared_w(0x002 / 2, 0x0b06)", self.hle)
        self.assertNotIn("nse3_final_b06_second_unknown", self.hle + self.phone)
        self.assertIn("bootstrap_preupload_profile::nse3_dsp_rom3_pair", self.hle)
        self.assertIn("peer_shared_w(0x004 / 2, 3)", self.hle)
        self.assertIn("peer_shared_w(0x006 / 2, 3)", self.hle)
        self.assertIn(
            "shared_006_write_cb().set(m_dsp_hle, "
            "FUNC(nokia_dsp_hle_device::shared_006_write_w))",
            self.phone,
        )

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
            "required_device<nokia_gsm_session_device> m_gsm_session",
            "required_device<nokia_lapdm_link_device> m_lapdm_link",
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

    def test_lapdm_owns_link_state_not_nokia_transport(self):
        for token in (
            "m_sapi", "m_downlink_send_sequence",
            "m_next_uplink_receive_sequence",
            "m_downlink_acknowledgement_pending",
            "m_pending_receive_sequence", "build_ua",
            "build_information_frame",
        ):
            self.assertIn(token, self.lapdm)
        for token in ("dspif", "enqueue_rx_packet", "RECEIVED_BLOCK", "m_reports_remaining"):
            self.assertNotIn(token, self.lapdm)
        self.assertIn("m_lapdm_link->receive_uplink", self.radio)
        self.assertIn("m_lapdm_link->build_ua", self.radio)
        self.assertIn("m_lapdm_link->build_information_frame", self.radio)
        self.assertIn("phase::location_update_acknowledgement", self.radio)
        self.assertIn("phase::channel_release_acknowledgement", self.radio)
        self.assertNotIn("block[1] = 0x73", self.radio)

    def test_gsm_session_owns_layer3_transaction_not_transport(self):
        for token in (
            "establish_layer3",
            "contention_resolution_delivered",
            "downlink_acknowledged",
            "awaiting_location_update_accept_acknowledgement",
            "awaiting_channel_release_acknowledgement",
            "m_pending_downlink",
        ):
            self.assertIn(token, self.session)
        for token in (
            "dspif", "enqueue_rx_packet", "RECEIVED_BLOCK",
            "BLOCK_REQUEST", "m_reports_remaining", "build_ua",
        ):
            self.assertNotIn(token, self.session)
        self.assertIn("m_gsm_session->establish_layer3", self.radio)
        self.assertIn("m_gsm_session->contention_resolution_delivered", self.radio)
        self.assertIn("m_gsm_session->downlink_acknowledged", self.radio)
        self.assertIn("m_gsm_session->pending_downlink", self.radio)

    def test_paging_ownership_is_split_at_decoded_blocks(self):
        for token in (
            "paging_fill", "paging_request", "subscriber_paging_group",
            "CCCH_BLOCK_OFFSETS",
        ):
            self.assertIn(token, self.network)
        for token in (
            "queue_incoming_page", "awaiting_paging_response",
            "awaiting_paging_contention_resolution",
            "m_registered_mobile_identity",
        ):
            self.assertIn(token, self.session)
        for token in (
            "serving_pch_report", "paging_frame_number",
            "m_page_after_registration", "m_pch_fill_delivered",
        ):
            self.assertIn(token, self.radio)
        self.assertNotIn("enqueue_rx_packet", self.network + self.session)

    def test_incoming_call_ownership_is_split_at_decoded_blocks(self):
        for token in (
            "cipher_mode_command", "mm_information", "incoming_call_setup",
            "traffic_assignment", "connect_acknowledge", "call_release",
        ):
            self.assertIn(token, self.network)
        for token in (
            "awaiting_cipher_mode_command_acknowledgement",
            "awaiting_mm_information_acknowledgement",
            "awaiting_incoming_call_setup_acknowledgement",
            "incoming_call_active", "awaiting_assignment_complete",
            "receive_layer3",
        ):
            self.assertIn(token, self.session)
        for token in (
            "phase::service_downlink", "phase::service_uplink_request",
            "phase::traffic_channel_change", "build_receive_ready",
            "information_indication",
        ):
            self.assertIn(token, self.radio)
        self.assertNotIn("enqueue_rx_packet", self.network + self.session)

    def test_incoming_sms_and_smart_message_ownership_is_split(self):
        for token in (
            "incoming_sms_cp_data", "incoming_smart_message_cp_data",
            "smart_message_multipart_part_capacity", "append(0xf5)",
            "append(0x7a)",
        ):
            self.assertIn(token, self.network)
        for token in (
            "incoming_service::smart_message",
            "awaiting_sms_sapi3_establishment",
            "awaiting_sms_cp_data_acknowledgement",
        ):
            self.assertIn(token, self.session)
        for token in (
            "m_incoming_smart_message_after_registration",
            "maximum_information_length",
        ):
            self.assertIn(token, self.radio)
        self.assertNotIn("dspif", self.network + self.session)

    def test_network_has_no_nokia_transport_ownership(self):
        for token in (
            "dspif", "enqueue_rx_packet", "CHANNEL_CHANGED_CNF",
            "m_reports_remaining", "build_ua", "m_downlink_send_sequence",
            "awaiting_location_update_accept_acknowledgement",
            "downlink_acknowledged",
        ):
            self.assertNotIn(token, self.network)


if __name__ == "__main__":
    unittest.main()
