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

    def test_outgoing_setup_requires_speech_and_called_party(self):
        setup = self.source.split(
            "m_state == u8(state::awaiting_outgoing_call_setup)", 1)[1]
        setup = setup.split("m_call_transaction = information[0];", 1)[0]
        self.assertIn("identifier == 0x04", setup)
        self.assertIn("identifier == 0x5e", setup)
        self.assertIn("!speech_bearer || !called_party", setup)


if __name__ == "__main__":
    unittest.main()
