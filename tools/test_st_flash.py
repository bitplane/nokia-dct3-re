import unittest

from tools.run_st_flash import OTP_COLD_PASS, OTP_PASS, RESULTS, check, check_otp

COMPLETE = "\n".join(RESULTS)


class StFlashCheckTests(unittest.TestCase):
    def test_complete(self):
        check(COMPLETE + "\n", 0)

    def test_incomplete_duplicate_or_failed(self):
        for text, code in (("", 0), (COMPLETE, 1), (COMPLETE + "\n" + COMPLETE, 0),
                           (COMPLETE + "\nFatal error: failed", 0)):
            with self.subTest(text=text, code=code), self.assertRaises(ValueError):
                check(text, code)

    def test_every_contract_field_is_required(self):
        for marker in RESULTS:
            for field in marker.split()[4:]:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    check(COMPLETE.replace(marker, marker.replace(field, field + "0")), 0)

    def test_each_stage_required_once_in_order(self):
        for marker in RESULTS:
            for text in (COMPLETE.replace(marker, ""), COMPLETE + "\n" + marker):
                with self.subTest(marker=marker, text=text), self.assertRaises(ValueError):
                    check(text, 0)
        with self.assertRaises(ValueError):
            check("\n".join(reversed(RESULTS)), 0)

    def test_otp_and_cold_results(self):
        for cold, marker in ((False, OTP_PASS), (True, OTP_COLD_PASS)):
            check_otp(marker + "\n", 0, cold=cold)
            for text, code in (("", 0), (marker, 1), (marker + "\n" + marker, 0),
                               (marker + "\nFatal error: failed", 0),
                               (OTP_PASS + "\n" + OTP_COLD_PASS, 0)):
                with self.subTest(cold=cold, text=text), self.assertRaises(ValueError):
                    check_otp(text, code, cold=cold)
            for field in marker.split()[4:]:
                with self.subTest(cold=cold, field=field), self.assertRaises(ValueError):
                    check_otp(marker.replace(field, field + "0"), 0, cold=cold)
