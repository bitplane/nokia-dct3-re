import unittest

try:
    from tools.nsm3d_live_verifier_check import check
except ModuleNotFoundError:
    from nsm3d_live_verifier_check import check


class LiveVerifierTests(unittest.TestCase):
    def fixture(self):
        return (
            "nsm3d_verifier_program: words=1234\n"
            "staged_dsp: publication word0=0000 word1=0006 word2=0006 word3=0006 pc=0f65 t=0.996196\n"
            "nsm3d_release: pc=002cb328 control=10 result0=0000 result1=0006 "
            "retained0=0000 retained1=0006 pairs0=58 pairs1=58 order_errors=0 t=0.996202\n"
            "nsm3d_loader_descriptor: address=00311d14 fields=fd00/ff80/027e/0500/0078/0000\n"
        )

    def test_native_result_retained(self):
        check(self.fixture(), bytes.fromhex("1234"))

    def test_wrong_upload(self):
        with self.assertRaises(ValueError):
            check(self.fixture(), bytes.fromhex("4321"))

    def test_wrong_declared_input_result(self):
        with self.assertRaises(ValueError):
            check(self.fixture().replace("word1=0006", "word1=0004"), bytes.fromhex("1234"))

    def test_missing_pair(self):
        with self.assertRaises(ValueError):
            check(self.fixture().replace("pairs1=58", "pairs1=57"), bytes.fromhex("1234"))

    def test_retention_before_publication(self):
        with self.assertRaises(ValueError):
            check(self.fixture().replace("t=0.996202", "t=0.1"), bytes.fromhex("1234"))

    def test_duplicate_publication(self):
        text = self.fixture()
        with self.assertRaises(ValueError):
            check(text + text.splitlines()[1], bytes.fromhex("1234"))


if __name__ == "__main__":
    unittest.main()
