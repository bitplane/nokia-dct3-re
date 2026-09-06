import unittest

from tools.radio_unreachable_forward_trace_check import verify


GOOD = """
gsm_ss: request=register transaction=1b invoke=1 service=2b number_length=5 active=1 t=1
radio_link: DOWNLINK_SIGNALLING_FAIL arfcn=1 count=0 t=2
dsp_hle: GSM incoming call forwarded before paging destination_length=5 condition=not-reachable t=3
gsm_call_adapter: incoming state id=1 epoch=1 phase=forwarded reason=not-reachable destination_length=5 t=4
"""


class RadioUnreachableForwardTraceCheckTest(unittest.TestCase):
    def test_accepts_post_loss_forwarding(self):
        verify(GOOD)

    def test_requires_cell_loss_before_routing(self):
        with self.assertRaisesRegex(ValueError, "event 2"):
            verify(GOOD.replace("DOWNLINK_SIGNALLING_FAIL", "LINK_OK"))

    def test_rejects_paging(self):
        with self.assertRaisesRegex(ValueError, "reached handset"):
            verify(GOOD + "GSM incoming page\n")


if __name__ == "__main__":
    unittest.main()
