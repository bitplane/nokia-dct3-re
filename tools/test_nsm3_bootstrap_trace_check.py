import unittest

from tools.nsm3_bootstrap_trace_check import check


class Nsm3BootstrapTest(unittest.TestCase):
    def setUp(self):
        self.text = "peer RAM W off=004 old=ffff data=0006\n" + "".join(
            f"dspif_transport: RAM W off={offset:03x} data=0000 t=0.5\n"
            for offset in [0xfe, 0x100] * 58)
        self.summary = {"final_pc": "002CADCE", "soft_resets": "0"}

    def test_frontier(self):
        check(self.text, self.summary)

    def test_missing_handoff(self):
        with self.assertRaises(ValueError):
            check(self.text.rsplit("\n", 2)[0], self.summary)

    def test_fabricated_completion(self):
        with self.assertRaises(ValueError):
            check(self.text + "bootstrap completion", self.summary)

    def test_wrong_frontier(self):
        with self.assertRaises(ValueError):
            check(self.text, {**self.summary, "final_pc": "00000000"})


if __name__ == "__main__":
    unittest.main()
