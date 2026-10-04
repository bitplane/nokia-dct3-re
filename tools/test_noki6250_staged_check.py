import unittest

from tools.noki6250_staged_check import check, check_silent_runtime


class StagedBoundaryTest(unittest.TestCase):
    def fixture(self):
        return ("release entry=0f00 words=223 prom_input=0006 clock=13000000 stage=verifier\n"
                "publication word0=0000 word1=0006 word2=0006 word3=0006 pc=0f65\n"
                "release entry=0f00 words=126 prom_input=0006 clock=13000000 stage=loader\n"
                "request selector=0014\n" + "request selector=0001\n" * 124 +
                "loader2_verified words=613 entry=0a00\n"
                "outside_uploaded_code pc=2c75\n"
                "observation_halt pc=2c75 ownership_retained=1\n")

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

    def runtime(self):
        commands = ["0000"] * 3 + ["8102", "900f", "8426", "920c", "920c", "920f", "920f"]
        return self.fixture() + "".join(
            f"6250_runtime_doorbell: pc=00429e48 command={command} argument=3fff pending=0001\n"
            for command in commands) + "6250_runtime_boundary: arm_pc=004c12ec dsp_pc=2c75 pending=0000\n"

    def test_silent_runtime(self):
        check_silent_runtime(self.runtime())

    def test_silent_runtime_rejects_peer_response(self):
        with self.assertRaises(ValueError):
            check_silent_runtime(self.runtime() + "RX enqueue\n")


if __name__ == "__main__":
    unittest.main()
