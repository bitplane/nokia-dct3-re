import unittest

from tools.nhm6_security_profile_check import validate


def comparison(transformed="d83b30d8", stored="d83b30d8"):
    return (
        "nhm6_security_verify: stage=compare input=00000004 "
        f"transformed={transformed} stored={stored} result=0 task=38 t=1.0\n")


class Nhm6SecurityProfileCheckTest(unittest.TestCase):
    def test_accepts_first_boot_and_settings_matches(self):
        validate(comparison() + comparison())

    def test_rejects_missing_comparison(self):
        with self.assertRaisesRegex(ValueError, "exactly two"):
            validate(comparison())

    def test_rejects_stale_pmm_verifier(self):
        with self.assertRaisesRegex(ValueError, "comparison 2"):
            validate(comparison() + comparison(stored="d43537dc"))

    def test_rejects_different_entered_code(self):
        with self.assertRaisesRegex(ValueError, "comparison 1"):
            validate(comparison(transformed="00112233") + comparison())


if __name__ == "__main__":
    unittest.main()
