import unittest

from tools.radio_3330_unsuitable_cell_trace_check import verify


BARRED = """
nhm6_bcch_parse: channel=50 block=1 data=49 06 1b 00 01 00 f1 10 00 01 40 00 00 00 00 00 02
TX packet type=57 payload=4 data=03050000
"""

RXLEV = """
nhm6_bcch_parse: channel=50 block=1 data=49 06 1b 00 01 00 f1 10 00 01 40 00 00 00 00 3f
TX packet type=57 payload=4 data=03050000
"""


class Radio3330UnsuitableCellTraceCheckTest(unittest.TestCase):
    def test_accepts_barred_cell_rejection(self):
        verify(BARRED, "barred")

    def test_accepts_rxlev_rejection(self):
        verify(RXLEV, "rxlev")

    def test_rejects_access_attempt(self):
        with self.assertRaisesRegex(ValueError, "CHANNEL REQUEST"):
            verify(BARRED + "TX packet type=0c payload=8\n", "barred")

    def test_rejects_loci_write(self):
        with self.assertRaisesRegex(ValueError, "EF_LOCI"):
            verify(
                RXLEV + "sim_device: update-binary fid=6f7e offset=4\n",
                "rxlev")

    def test_rejects_wrong_profile_evidence(self):
        with self.assertRaisesRegex(ValueError, "cell-suitability evidence"):
            verify(RXLEV, "barred")


if __name__ == "__main__":
    unittest.main()
