import unittest

from tools.radio_3330_boundary_trace_check import verify


GOOD = """
TX packet type=56 payload=160 data=0337ffffffff
TX packet type=02 payload=20 data=04120200000000505000033700000000
nhm6_bcch_parse: channel=50 block=0012 data=31061c00f1100001
nhm6_bcch_parse: channel=50 block=0013 data=59061a000000
nhm6_bcch_parse: channel=50 block=0014 data=49061b000100f1100001
nhm6_bcch_parse: channel=50 block=0015 data=5906198f9b800000
nhm6_si_terminal: message=19 changed=01 state=0011
TX packet type=02 payload=20 data=04120209000000106000033710000000
radio_si_selector_insert: firmware=nhm6 input=0012 type=03ec
radio_si_after_advance: firmware=nhm6 selected_si=0013 selected_type=03ec
TX packet type=46 payload=8 data=3210321000010000
"""


class Radio3330BoundaryTraceCheckTest(unittest.TestCase):
    def test_accepts_evidenced_automatic_access_frontier(self):
        verify(GOOD)

    def test_rejects_missing_dcs_si1(self):
        with self.assertRaisesRegex(ValueError, "DCS SI1"):
            verify(GOOD.replace(
                "nhm6_bcch_parse: channel=50 block=0015 "
                "data=5906198f9b800000\n", ""))

    def test_rejects_incomplete_si_publication(self):
        with self.assertRaisesRegex(ValueError, "incomplete-SI"):
            verify(GOOD + "TX packet type=57 payload=4 data=03050000\n")

    def test_rejects_missing_si4_selection(self):
        with self.assertRaisesRegex(ValueError, "accepted SI4"):
            verify(GOOD.replace(
                "radio_si_after_advance: firmware=nhm6 "
                "selected_si=0013 selected_type=03ec\n", ""))


if __name__ == "__main__":
    unittest.main()
