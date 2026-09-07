#!/usr/bin/env python3

import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class GsmSubscriberProfileTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.profile = (ROOT / "driver/gsm_subscriber.h").read_text()
        cls.phone = (ROOT / "driver/nokia_dct3.cpp").read_text()
        cls.card = (ROOT / "driver/nokia_sim_card.cpp").read_text()
        cls.network = (ROOT / "driver/nokia_gsm_network.cpp").read_text()

    def test_one_product_profile_configures_both_devices(self):
        self.assertIn("gsm::subscriber::profile subscriber", self.phone)
        self.assertEqual(
            self.phone.count("set_subscriber_profile(product.subscriber)"), 2
        )

    def test_profile_owns_immutable_subscription_contract(self):
        for field in (
            "iccid",
            "imsi",
            "preferred_plmn",
            "service_provider_name",
            "access_control_class",
            "home_location",
            "authentication",
            "ki",
        ):
            self.assertIn(field, self.profile)

    def test_old_per_device_constants_and_setter_are_absent(self):
        self.assertNotIn("static constexpr u8 imsi[]", self.card)
        self.assertNotIn("laboratory_ki", self.network)
        self.assertNotIn("set_authentication(", self.phone)

    def test_runtime_card_records_are_not_profile_fields(self):
        for field in ("m_loci", "m_kc", "m_adn", "m_sms"):
            self.assertNotIn(field, self.profile)


if __name__ == "__main__":
    unittest.main()
