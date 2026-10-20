import unittest

from tools.run_sim_nvram_atomic import cases


class SimStorageCasesTest(unittest.TestCase):
    def test_legacy_prefixes_preserve_only_supported_fields(self):
        default = bytes([0xff]) * 3570
        fixtures = cases(default)
        for name, size in (("legacy_files", 1636), ("legacy_sms", 3484),
                           ("legacy_security", 3521), ("legacy_acm", 3524),
                           ("legacy_plmn", 3560), ("legacy_smss", 3562)):
            payload, expected, rejected = fixtures[name]
            self.assertEqual(len(payload), size)
            self.assertEqual(expected, payload + default[size:])
            self.assertFalse(rejected)
        self.assertEqual(fixtures["current"][0], fixtures["current"][1])

    def test_rejected_files_expect_whole_default_not_prefix(self):
        default = bytes([0xff]) * 3570
        fixtures = cases(default)
        negatives = {name for name, (_, _, rejected) in fixtures.items() if rejected}
        self.assertEqual(negatives, {"short_adn", "short_sms", "short_security",
                                    "short_acm", "short_preferred_plmn",
                                    "short_forbidden_plmn", "short_smss", "short_acm_max",
                                    "short_puct", "trailing", "invalid_enabled",
                                    "fdn_short", "fdn_trailing", "fdn_bad_version",
                                    "fdn_bad_capability", "fdn_bad_validity", "fdn_bad_access", "fdn_bad_reserved"})
        for name in negatives:
            self.assertEqual(fixtures[name][1], default)
            self.assertEqual(fixtures[name][0][0], 0x41)
        self.assertEqual(fixtures["invalid_enabled"][0][3520], 2)

    def test_layout_change_requires_explicit_fixture_review(self):
        for size in (0, 3521, 3523, 3524, 3525, 3559, 3561):
            with self.subTest(size=size), self.assertRaises(ValueError):
                cases(bytes(size))


if __name__ == "__main__":
    unittest.main()
