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
    def test_physical_release_keeps_media_and_epoch_requirements(self):
        physical = TRACE.replace(
            'gsm_call_adapter: termination id=1 cause=16 result=accepted\n'
            'GSM service downlink kind=13 sapi=0 pd=03 message=25 length=5',
            '5210_pin_state_physical: key=End\n'
            'GSM service uplink sapi=0 pd=03 message=25\n'
            'GSM service downlink kind=25 sapi=0 pd=03 message=2d\n'
            'GSM service uplink sapi=0 pd=03 message=2a').replace(
            'LAPDm service Channel Release acknowledged',
            'LAPDm service Channel Release acknowledged\n'
            'gsm_call_adapter: state id=1 epoch=2 phase=ended')
        check(physical, 2, physical_end=True)
        late = 'gsm_call_adapter: media direction=downlink id=1 sequence=41 result=accepted'
        with self.assertRaisesRegex(ValueError, 'too few'):
            check(physical.replace(late, '') + late, 2, physical_end=True)
        for line in physical.strip().splitlines():
            with self.subTest(line=line), self.assertRaises(ValueError):
                check(physical.replace(line, '', 1), 2, physical_end=True)

    def test_accepts_contiguous_restored_media(self):
        check(TRACE, 2)

    def test_rejects_sequence_gap(self):
        with self.assertRaisesRegex(ValueError, "not contiguous"):
            check(TRACE.replace("sequence=41", "sequence=42"), 2)


if __name__ == "__main__":
    unittest.main()
