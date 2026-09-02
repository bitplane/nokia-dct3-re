import unittest

from tools.radio_outgoing_host_media_trace_check import check


TRACE = """
gsm_call_adapter: state id=1 epoch=1 phase=connected
gsm_call_adapter: media direction=uplink id=1 sequence=0 good=0
gsm_call_adapter: media direction=uplink id=1 sequence=1 good=1
gsm_call_adapter: media direction=downlink id=1 sequence=0 result=accepted
gsm_call_adapter: media direction=downlink id=1 sequence=1 result=accepted
gsm_call_adapter: termination id=1 cause=16 result=accepted
GSM service downlink kind=13 sapi=0 pd=03 message=25 length=5
LAPDm service Channel Release acknowledged
gsm_call_adapter: media direction=downlink id=1 sequence=200 result=rejected
PCH no-identity fill
"""


class HostMediaTraceCheckTest(unittest.TestCase):
    def test_accepts_contiguous_media(self):
        check(TRACE, 2)

    def test_accepts_clean_standalone_release(self):
        check(TRACE.replace(
            "gsm_call_adapter: media direction=downlink id=1 sequence=200 "
            "result=rejected\n", ""), 2, standalone=True)

    def test_rejects_sequence_gap(self):
        with self.assertRaises(ValueError):
            check(TRACE.replace("sequence=1", "sequence=2"), 2)


if __name__ == "__main__":
    unittest.main()
