import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
SESSION = (ROOT / "driver/nokia_gsm_session.cpp").read_text()
NETWORK_H = (ROOT / "driver/nokia_gsm_network.h").read_text()
NETWORK_CPP = (ROOT / "driver/nokia_gsm_network.cpp").read_text()
SMS_TRANSPORT = (ROOT / "driver/gsm_sms_transport.cpp").read_text()


class GsmSmsSessionInvariantTest(unittest.TestCase):
    def test_cp_ack_precedes_terminal_rp_response(self):
        self.assertIn(
            "m_sms_cp_data_acknowledged &&\n"
            "\t\t\t\t(sms.kind == gsm::sms::uplink_kind::rp_ack ||",
            SESSION)
        self.assertIn(
            "m_sms_cp_data_acknowledged = true;",
            SESSION)

    def test_rp_error_closes_without_admitting_successor(self):
        self.assertIn(
            "sms.kind == gsm::sms::uplink_kind::rp_error",
            SESSION)
        self.assertIn("const bool more_sms_deliveries", SESSION)
        self.assertIn("m_sms_delivery_index + 1", SESSION)
        self.assertIn("next.data[4] == m_sms_rp_reference", SESSION)
        self.assertIn("std::equal(", SESSION)
        self.assertIn("duplicate queued rp_reference=%02x", SESSION)

    def test_transaction_correlation_and_phase_are_saved(self):
        for field in (
                "m_sms_cp_transaction",
                "m_sms_rp_reference",
                "m_sms_cp_data_acknowledged",
                "m_sms_rp_acknowledged"):
            self.assertIn(f"save_item(NAME({field}));", SESSION)

    def test_malformed_sms_is_rejected_before_paging(self):
        radio = (ROOT / "driver/nokia_radio_peer.cpp").read_text()
        self.assertIn("incoming_sms_admissible", NETWORK_CPP)
        self.assertIn("gsm::sms::parse_deliver(", NETWORK_CPP)
        self.assertIn("address_digits > 20", SMS_TRANSPORT)
        self.assertIn(
            "udl_index + 1 + required_octets != length",
            SMS_TRANSPORT)
        self.assertIn("incoming_service_admissible(service)", radio)
        self.assertIn("GSM incoming service rejected before ", radio)
        self.assertIn('"paging service=%u', radio)

    def test_release_distinguishes_registration_one_shot_and_queued_work(self):
        radio = (ROOT / "driver/nokia_radio_peer.cpp").read_text()
        self.assertIn("incoming_service_completed()", radio)
        self.assertIn("!m_gsm_session->incoming_service_queued()", radio)
        self.assertIn(
            "save_item(NAME(m_incoming_service_completed));",
            SESSION)

    def test_multipart_generation_is_bounded_and_rejects_invalid_index(self):
        self.assertIn("smart_message_maximum_parts = 3", NETWORK_H)
        self.assertIn(
            "part_index >= delivered_part_count ||\n"
            "\t\t\tdeclared_part_count > smart_message_maximum_parts",
            NETWORK_CPP)

    def test_application_frontier_profiles_are_network_compositions(self):
        driver = (ROOT / "driver/nokia_dct3.cpp").read_text()
        for profile in (
                "missing_second_part", "mismatched_reference",
                "incorrect_total", "duplicate_first_part",
                "second_part_first", "wrong_destination_port",
                "truncated_udh", "invalid_rtpl_command",
                "missing_rtpl_terminator", "stale_then_valid"):
            self.assertIn(f"smart_message_profile::{profile}", driver)
            self.assertIn(f"smart_message_profile::{profile}", NETWORK_CPP)
        self.assertIn('PORT_START("SMARTCFG")', driver)


if __name__ == "__main__":
    unittest.main()
