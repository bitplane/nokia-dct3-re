import unittest

from tools.run_st_flash import PASS, check


class StFlashCheckTests(unittest.TestCase):
    def test_complete(self):
        check(PASS + "\n", 0)

    def test_incomplete_duplicate_or_failed(self):
        for text, code in (("", 0), (PASS, 1), (PASS + "\n" + PASS, 0),
                           (PASS + "\nFatal error: failed", 0)):
            with self.subTest(text=text, code=code), self.assertRaises(ValueError):
                check(text, code)

    def test_every_contract_field_is_required(self):
        for field in PASS.split()[4:]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                check(PASS.replace(field, field + "0"), 0)
