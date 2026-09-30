import unittest

from tools.npe3_bootstrap_trace_check import check


class Npe3BootstrapTest(unittest.TestCase):
    def setUp(self):
        self.text = (
            "gensio: W off=2d data=22 old=22 pc=004ec7b0\n"
            "gensio: R off=6d data=07 pc=004ec7bc\n" + "".join(
                f"dspif_transport: RAM W off={offset:03x} data=0000 t=0.5\n"
                for offset in [0xfe, 0x100] * 116))
        self.summary = {"final_pc": "00426CC8", "soft_resets": "0"}

    def test_frontier(self):
        check(self.text, self.summary)

    def test_missing_handoff(self):
        with self.assertRaises(ValueError):
            check(self.text.rsplit("\n", 2)[0], self.summary)

    def test_wrong_receive_ready(self):
        with self.assertRaises(ValueError):
            check(self.text.replace("data=07", "data=03"), self.summary)

    def test_fabricated_completion(self):
        with self.assertRaises(ValueError):
            check(self.text + "bootstrap completion", self.summary)

    def test_wrong_frontier(self):
        with self.assertRaises(ValueError):
            check(self.text, {**self.summary, "final_pc": "004DC0FC"})

    def test_reset(self):
        with self.assertRaises(ValueError):
            check(self.text, {**self.summary, "soft_resets": "1"})


if __name__ == "__main__":
    unittest.main()
