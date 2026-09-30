import unittest

from tools.nse5_bootstrap_trace_check import check


class BootstrapTest(unittest.TestCase):
    def setUp(self):
        self.text = "\n".join(
            f"dspif_transport: RAM W off={offset:03x} data=0000 t=0.1"
            for offset in [0xfe, 0x100] * 114)
        self.summary = {"final_pc": "00432F9A", "soft_resets": "0"}

    def test_exact_frontier(self):
        check(self.text, self.summary)

    def test_missing_handoff(self):
        with self.assertRaisesRegex(ValueError, "228"):
            check(self.text.rsplit("\n", 1)[0], self.summary)

    def test_wrong_order(self):
        with self.assertRaisesRegex(ValueError, "alternating"):
            check(self.text.replace("off=0fe", "off=100", 1), self.summary)

    def test_guess_is_not_boot_evidence(self):
        with self.assertRaisesRegex(ValueError, "unvalidated"):
            check(self.text + "\nbootstrap publication offset=002 value=0001", self.summary)

    def test_wrong_boundary(self):
        with self.assertRaisesRegex(ValueError, "wait"):
            check(self.text, {**self.summary, "final_pc": "0049FFFA"})

    def test_reset_rejected(self):
        with self.assertRaisesRegex(ValueError, "reset"):
            check(self.text, {**self.summary, "soft_resets": "01"})
