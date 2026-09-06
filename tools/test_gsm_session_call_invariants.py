from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class GsmSessionCallInvariantTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header = (ROOT / "driver/nokia_gsm_session.h").read_text()
        cls.source = (ROOT / "driver/nokia_gsm_session.cpp").read_text()

    def test_assignment_idempotence_is_generic_saved_session_state(self):
        self.assertIn("bool m_traffic_assignment_issued = false;", self.header)
        self.assertIn(
            "save_item(NAME(m_traffic_assignment_issued));", self.source)
        call_confirmed = self.source.split("if (message_type == 0x08)", 1)[1]
        call_confirmed = call_confirmed.split("if (message_type == 0x01)", 1)[0]
        self.assertIn("m_traffic_assignment_issued", call_confirmed)
        self.assertIn("m_traffic_assignment_issued = true;", call_confirmed)
        for product in ("noki", "NHM", "NSE"):
            self.assertNotIn(product, call_confirmed)

    def test_assignment_guard_resets_only_at_call_boundaries(self):
        self.assertIn(
            "m_incoming_service = u8(service);\n"
            "\tm_traffic_assignment_issued = false;",
            self.source,
        )
        release = self.source.split(
            "if (m_state == u8(state::awaiting_release_complete)", 1)[1]
        self.assertIn("m_traffic_assignment_issued = false;", release)

    def test_mobile_originated_transaction_state_is_saved(self):
        for field in ("m_mobile_originated_call", "m_call_transaction"):
            self.assertIn(f"save_item(NAME({field}));", self.source)
        self.assertIn(
            "m_network->call_proceeding(m_call_transaction)", self.source)
        self.assertIn(
            "m_network->call_alerting(m_call_transaction)", self.source)
        self.assertIn(
            "m_network->call_connect(m_call_transaction)", self.source)

    def test_outgoing_request_is_saved_and_product_independent(self):
        for field in (
            "m_outgoing_request_pending",
            "m_outgoing_request_id",
            "m_outgoing_called_digits",
            "m_outgoing_called_digits_length",
        ):
            self.assertIn(f"save_item(NAME({field}));", self.source)
        request = self.source.split(
            "m_outgoing_request_pending = true;", 1)[0]
        request = request.rsplit(
            "m_state == u8(state::awaiting_outgoing_call_setup)", 1)[1]
        for product in ("noki", "NHM", "NSE"):
            self.assertNotIn(product, request)

    def test_network_outcomes_are_generic_and_standards_shaped(self):
        compact_source = "".join(self.source.split())
        for outcome in ("busy", "no_answer", "service_reject"):
            self.assertIn(
                f"outgoing_call_outcome::{outcome}", compact_source)
        self.assertIn(
            "m_network->call_disconnect(m_call_transaction, 0x11)",
            self.source,
        )
        for product in ("noki3210", "noki3310", "noki3330", "noki3410",
                        "NHM-", "NSE-"):
            self.assertNotIn(
                product,
                self.source.split("configured_outgoing_call_outcome", 1)[1],
            )

    def test_asynchronous_decision_queue_is_saved_and_id_correlated(self):
        for field in (
            "m_outgoing_decision_accepted",
            "m_outgoing_decision",
            "m_outgoing_decision_request_id",
            "m_outgoing_policy_request_id",
        ):
            self.assertIn(f"save_item(NAME({field}));", self.source)
        submit = self.source.split(
            "bool nokia_gsm_session_device::submit_outgoing_decision", 1)[1]
        submit = submit.split(
            "nokia_gsm_session_device::apply_outgoing_decision", 1)[0]
        self.assertIn("request_id != m_outgoing_request_id", submit)
        self.assertIn("m_outgoing_decision_accepted", submit)
        self.assertIn("service_reject", submit)
        self.assertIn("state::awaiting_outgoing_decision", submit)
        self.assertIn("TIMER_CALLBACK_MEMBER", self.source)

    def test_host_termination_is_saved_correlated_and_network_owned(self):
        for field in (
            "m_outgoing_termination_accepted",
            "m_outgoing_termination_request_id",
            "m_outgoing_termination_cause",
        ):
            self.assertIn(f"save_item(NAME({field}));", self.source)

    def test_release_save_gate_is_observational_saved_state(self):
        self.assertIn("bool m_release_waiting = false;", self.header)
        self.assertIn("save_item(NAME(m_release_waiting));", self.source)
        self.assertIn(
            '"nokia_gsm_call_release_waiting_handset"', self.source
        )
        transition = self.source.split(
            "state::awaiting_network_disconnect_acknowledgement", 1
        )[1].split("return downlink_kind::none;", 1)[0]
        self.assertIn("m_state = u8(state::awaiting_handset_release)", transition)
        self.assertIn("m_release_waiting = true", transition)
        submit = self.source.split(
            "bool nokia_gsm_session_device::submit_outgoing_termination", 1
        )[1].split(
            "nokia_gsm_session_device::apply_outgoing_termination", 1
        )[0]
        self.assertIn("request_id != m_outgoing_request_id", submit)
        self.assertIn("!m_outgoing_decision_accepted", submit)
        self.assertIn("m_mobile_originated_call", submit)
        apply = self.source.split(
            "nokia_gsm_session_device::apply_outgoing_termination", 1
        )[1].split(
            "nokia_gsm_session_device::apply_outgoing_decision", 1
        )[0]
        self.assertIn("m_network->call_disconnect", apply)
        self.assertIn("state::awaiting_network_disconnect_acknowledgement", apply)

    def test_async_network_clear_enters_generic_radio_downlink_scheduler(self):
        radio = (ROOT / "driver/nokia_radio_peer.cpp").read_text()
        tick = radio.split(
            "void nokia_radio_peer_device::tick()", 1
        )[1].split(
            "bool nokia_radio_peer_device::fast_completion_pending", 1
        )[0]
        self.assertIn("m_gsm_session->pending_downlink_kind()", tick)
        self.assertIn("phase::service_uplink_request", tick)
        self.assertIn("phase::service_downlink", tick)
        for product in ("noki3210", "noki3310", "noki3330", "noki3410",
                        "NHM-", "NSE-"):
            self.assertNotIn(product, tick)

    def test_non_call_cm_service_and_malformed_identity_fail_closed(self):
        establishment = self.source.split(
            "const bool cm_service_request =", 1)[1]
        establishment = establishment.split(
            "m_established_layer3.fill(0);", 1)[0]
        self.assertIn("(information[2] & 0x0f) == 0x01", establishment)
        self.assertIn("identity_length == 0 || identity_length > 8", establishment)
        self.assertIn(
            "identity_length_offset + 1 + identity_length > length",
            establishment,
        )

    def test_conditional_forwarding_is_network_owned_and_saved(self):
        network_header = (ROOT / "driver/nokia_gsm_network.h").read_text()
        network_source = (ROOT / "driver/nokia_gsm_network.cpp").read_text()
        radio = (ROOT / "driver/nokia_radio_peer.cpp").read_text()
        for field in (
            "m_forwarding_registered",
            "m_forwarding_active",
            "m_forwarding_number_length",
            "m_forwarding_number",
            "m_forwarding_basic_service",
            "m_forwarding_basic_service_code",
            "m_forwarding_no_reply_time",
        ):
            self.assertIn(f"save_item(NAME({field}));", network_source)
        for condition in ("unconditional", "busy", "no_reply", "not_reachable"):
            self.assertIn(condition, network_header)
        reset = network_source.split(
            "void nokia_gsm_network_device::device_reset()", 1)[1].split(
                "void nokia_gsm_network_device::set_mobility_profile", 1)[0]
        for field in (
            "m_forwarding_registered",
            "m_forwarding_active",
            "m_forwarding_number",
        ):
            self.assertNotIn(field, reset)
        self.assertIn("host_incoming_result::forwarded", radio)
        forwarding = radio.split(
            "queue_host_incoming_call", 1)[1].split(
                "nokia_radio_peer_device::nokia_radio_peer_device", 1)[0]
        self.assertLess(
            forwarding.index("incoming_service_diverted"),
            forwarding.index("set_incoming_caller"),
        )

    def test_multiparty_state_and_no_reply_timer_are_saved(self):
        for field in (
            "m_multiparty_active",
            "m_multiparty_held",
            "m_multiparty_operation_count",
            "m_incoming_call_forwarded",
        ):
            self.assertIn(f"save_item(NAME({field}));", self.source)
        for operation in ("build_mpty", "hold_mpty", "retrieve_mpty",
                          "split_mpty"):
            self.assertIn(f"operation::{operation}", self.source)
        self.assertIn("TIMER_CALLBACK_MEMBER(nokia_gsm_session_device::no_reply_timer)",
                      self.source)

    def test_host_forwarding_reports_condition_and_destination(self):
        adapter_header = (ROOT / "driver/nokia_gsm_call_adapter.h").read_text()
        adapter_source = (ROOT / "driver/nokia_gsm_call_adapter.cpp").read_text()
        radio_header = (ROOT / "driver/nokia_radio_peer.h").read_text()
        radio_source = (ROOT / "driver/nokia_radio_peer.cpp").read_text()
        self.assertIn("last_host_forwarding_condition()", radio_header)
        self.assertIn(
            "save_item(NAME(m_last_host_forwarding_condition));", radio_source)
        self.assertIn('writer.Key("forwarding_reason");', adapter_source)
        self.assertIn('writer.Key("forwarding_destination");', adapter_source)
        self.assertIn("required_device<nokia_gsm_network_device> m_network;",
                      adapter_header)

    def test_ussd_outcomes_are_network_owned_and_fail_closed(self):
        network_header = (ROOT / "driver/nokia_gsm_network.h").read_text()
        driver = (ROOT / "driver/nokia_dct3.cpp").read_text()
        for outcome in ("success", "return_error", "reject", "silence"):
            self.assertIn(outcome, network_header)
        self.assertIn('m_ussd_config(*this, "USSDCFG")', driver)
        ussd = self.source.split(
            "operation::process_uss_request", 1)[1].split(
                "request.operation_code == gsm::ss::operation::register_ss", 1)[0]
        self.assertIn("configured_ussd_outcome", ussd)
        self.assertIn("gsm::ss::error_result(request, 0x22)", ussd)
        self.assertIn("gsm::ss::reject_result(request, 0x00)", ussd)

    def test_outgoing_setup_requires_speech_and_called_party(self):
        setup = self.source.split(
            "m_state == u8(state::awaiting_outgoing_call_setup)", 1)[1]
        setup = setup.split("m_call_transaction = information[0];", 1)[0]
        self.assertIn("identifier == 0x04", setup)
        self.assertIn("identifier == 0x5e", setup)
        self.assertIn("!speech_bearer || !called_party", setup)
        self.assertIn("m_outgoing_called_digits_length == 0", setup)


if __name__ == "__main__":
    unittest.main()
