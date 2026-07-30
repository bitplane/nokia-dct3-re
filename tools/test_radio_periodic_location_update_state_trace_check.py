import unittest

from tools.radio_periodic_location_update_state_trace_check import check
PREFIX = """
dspif_transport: RX enqueue type=80 payload=34 data=5012000000010001000049061b000100f11000014000010000000000002b2b2b2b2b t=12.0
dsp_hle: GSM service establish sapi=0 pd=05 message=08 length=18 data=05087000f000fffe33080910101032547698 t=16.5
dsp_hle: LAPDm Location Updating Accept acknowledged nr=1 t=16.6
dsp_hle: LAPDm Channel Release acknowledged nr=2 t=16.7
"""
REPLAY = """
dsp_hle: TX packet type=0c payload=8 radio_phase=random_access data=0000096b00000000 t=402.9
dsp_hle: GSM service establish sapi=0 pd=05 message=08 length=18 data=05087100f110000133080910101032547698 t=403.1
dsp_hle: LAPDm Location Updating Accept acknowledged nr=1 t=403.2
dsp_hle: LAPDm Channel Release acknowledged nr=2 t=403.3
"""
SUFFIX = """
dspif_transport: RX enqueue type=80 payload=34 data=601200000005000100001506210001f02b2b t=404.0
"""


def replay_log(restored: str = REPLAY) -> str:
    return (
        PREFIX
        + "state_replay: phase=reference event=begin t=398.000000\n"
        + REPLAY
        + "state_replay: phase=reference event=end t=410.000000\n"
        + "state_roundtrip: result=pass timer_delta=0000 mode=1234 "
          "requested_at=398.000000 t=398.000000\n"
        + "state_replay: phase=restored event=begin t=398.000000\n"
        + restored
        + "state_replay: phase=restored event=end t=410.000000\n"
        + SUFFIX
    )


class PeriodicLocationUpdateStateTraceCheckTest(unittest.TestCase):
    def test_accepts_identical_expiry_replay(self):
        check(replay_log())

    def test_rejects_divergent_periodic_request(self):
        with self.assertRaisesRegex(ValueError, "diverged"):
            check(replay_log(REPLAY.replace("05087100", "05087000")))

    def test_rejects_failed_roundtrip(self):
        with self.assertRaisesRegex(ValueError, "save/load"):
            check(replay_log().replace(
                "state_roundtrip: result=pass",
                "state_roundtrip: result=fail"))


if __name__ == "__main__":
    unittest.main()
