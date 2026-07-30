import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
SESSION = (ROOT / "driver/nokia_gsm_session.cpp").read_text()
NETWORK_H = (ROOT / "driver/nokia_gsm_network.h").read_text()
NETWORK_CPP = (ROOT / "driver/nokia_gsm_network.cpp").read_text()


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
        self.assertIn(
            "m_sms_rp_acknowledged &&\n"
            "\t\t\t\tm_smart_message_part_index + 1",
            SESSION)

    def test_transaction_correlation_and_phase_are_saved(self):
        for field in (
                "m_sms_cp_transaction",
                "m_sms_rp_reference",
                "m_sms_cp_data_acknowledged",
                "m_sms_rp_acknowledged"):
            self.assertIn(f"save_item(NAME({field}));", SESSION)

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
            "part_index >= part_count || "
            "part_count > smart_message_maximum_parts",
            NETWORK_CPP)


if __name__ == "__main__":
    unittest.main()
