import unittest

from tools.noki6250_staged_check import check


class StagedBoundaryTest(unittest.TestCase):
    def fixture(self):
        return ("release entry=0f00 words=223 prom_input=0006 clock=13000000 stage=verifier\n"
                "publication word0=0000 word1=0006 word2=0006 word3=0006 pc=0f65\n"
                "release entry=0f00 words=126 prom_input=0006 clock=13000000 stage=loader\n"
                "request selector=0014\n" + "request selector=0001\n" * 124 +
                "loader2_verified words=613 entry=0a00\n"
                "unavailable_program address=2c75 pc=2c76 stage=loader\n")

    def test_reviewed_boundary(self):
        check(self.fixture())

    def test_missing_verification(self):
        with self.assertRaises(ValueError):
            check(self.fixture().replace("loader2_verified", "not_verified"))

    def test_other_product_sequence(self):
        with self.assertRaises(ValueError):
            check(self.fixture() + "request selector=0001\n")

    def test_no_forced_result(self):
        with self.assertRaises(ValueError):
            check(self.fixture().replace("word0=0000", "word0=0001"))


if __name__ == "__main__":
    unittest.main()
