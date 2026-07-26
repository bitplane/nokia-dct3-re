import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class GsmAuthenticationSplitTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.network = (ROOT / "driver/nokia_gsm_network.cpp").read_text()
        cls.network_header = (ROOT / "driver/nokia_gsm_network.h").read_text()
        cls.session = (ROOT / "driver/nokia_gsm_session.cpp").read_text()
        cls.session_header = (ROOT / "driver/nokia_gsm_session.h").read_text()
        cls.card = (ROOT / "driver/nokia_sim_card.cpp").read_text()
        cls.phone = (ROOT / "driver/nokia_dct3.cpp").read_text()
        cls.radio = (ROOT / "driver/nokia_radio_peer.cpp").read_text()

    def test_network_owns_challenge_and_expected_response(self):
        for token in (
            "authentication_request() const",
            "authentication_response_valid(",
            "gsm::a3a8::aes_example(laboratory_ki(), rand)",
            "authentication_reject() const",
        ):
            self.assertIn(token, self.network + self.network_header)
        self.assertNotIn("nokia_sim_card", self.network)

    def test_session_owns_saved_authentication_states(self):
        for token in (
            "awaiting_authentication_request_acknowledgement",
            "awaiting_authentication_response",
            "awaiting_authentication_reject_acknowledgement",
            "m_network->authentication_response_valid",
        ):
            self.assertIn(token, self.session + self.session_header)
        self.assertIn("save_item(NAME(m_state))", self.session)

    def test_policy_is_opt_in_and_card_key_matches_network(self):
        self.assertIn("bool m_authentication_required = false", self.session_header)
        self.assertIn('m_authentication_config(*this, "AUTHCFG")', self.phone)
        self.assertIn(
            "nokia_gsm_network_device::laboratory_ki()", self.phone
        )
        self.assertNotIn("noki6110", self.card)

    def test_radio_does_not_promote_rejected_registration(self):
        self.assertIn(
            "m_gsm_session->registered_mobile_identity_length() == 8",
            self.radio,
        )


if __name__ == "__main__":
    unittest.main()
