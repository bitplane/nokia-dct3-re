#!/usr/bin/env python3

import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class LapdmExpiryContractTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header = (ROOT / "driver/nokia_lapdm_link.h").read_text()
        cls.link = (ROOT / "driver/nokia_lapdm_link.cpp").read_text()
        cls.radio = (ROOT / "driver/nokia_radio_peer.cpp").read_text()
        cls.session = (ROOT / "driver/nokia_gsm_session.cpp").read_text()

    def test_t200_and_n200_are_explicit_control_channel_contracts(self):
        self.assertIn("t200_control_frames = 217", self.header)
        self.assertIn("n200_control_attempts = 3", self.header)
        self.assertIn("m_last_downlink_frame[sapi] = frame", self.link)
        self.assertIn("return m_last_downlink_frame[sapi]", self.link)

    def test_all_mutable_expiry_state_is_saved(self):
        for field in (
            "m_transaction_frames",
            "m_transaction_attempts",
            "m_retransmission_pending",
            "m_last_downlink_frame",
            "m_reassembly_frames",
        ):
            self.assertIn(f"save_item(NAME({field}))", self.link)

    def test_partial_uplink_reassembly_has_a_bounded_lifetime(self):
        self.assertIn(
            "m_reassembly_frames = m_layer3_more_data ? t200_control_frames : 0",
            self.link,
        )
        self.assertIn("expiry_kind::reassembly_failure", self.link)
        self.assertIn("m_layer3_information.fill(0)", self.link)

    def test_radio_clock_drives_expiry_and_session_owns_recovery(self):
        self.assertIn("m_lapdm_link->advance_frame()", self.radio)
        self.assertIn("m_lapdm_link->take_retransmission()", self.radio)
        self.assertIn("m_gsm_session->radio_link_failed()", self.radio)
        self.assertIn("const auto identity = m_registered_mobile_identity", self.session)
        self.assertIn("m_registered_mobile_identity = identity", self.session)


if __name__ == "__main__":
    unittest.main()
