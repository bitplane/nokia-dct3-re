import unittest

from tools.radio_outgoing_host_media_restore_trace_check import check


TRACE = """
state_roundtrip: result=pass
gsm_call_adapter: state id=1 epoch=2 phase=connected media_uplink_sequence=41 media_downlink_sequence=40
gsm_call_adapter: stale epoch=1 current=2 type=media id=1 result=rejected
gsm_call_adapter: media direction=downlink id=1 sequence=40 result=accepted
gsm_call_adapter: media direction=downlink id=1 sequence=41 result=accepted
gsm_call_adapter: termination id=1 cause=16 result=accepted
GSM service downlink kind=13 sapi=0 pd=03 message=25 length=5
LAPDm service Channel Release acknowledged
PCH no-identity fill
"""


class HostMediaRestoreTraceCheckTests(unittest.TestCase):
    def test_accepts_contiguous_restored_media(self):
        check(TRACE, 2)

    def test_rejects_sequence_gap(self):
        with self.assertRaisesRegex(ValueError, "not contiguous"):
            check(TRACE.replace("sequence=41", "sequence=42"), 2)


if __name__ == "__main__":
    unittest.main()
