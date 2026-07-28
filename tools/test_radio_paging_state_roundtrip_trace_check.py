import unittest

from tools.radio_paging_state_roundtrip_trace_check import check
from tools.test_radio_paging_trace_check import GOOD


RECORD = (
    "dsp_hle: PCH IMSI page transmitted channel=60 fn=3759\n"
    "dsp_hle: radio peer RX type=80 sequence=1")


def replay_log(restored: str = RECORD) -> str:
    page = "dsp_hle: PCH IMSI page transmitted channel=60 fn=3759\n"
    prefix, suffix = GOOD.split(page, 1)
    return (
        prefix
        + "state_replay: phase=reference event=begin t=11.800000\n"
        + RECORD + "\n"
        + "state_replay: phase=reference event=end t=12.800000\n"
        + "state_replay: phase=restored event=begin t=11.800000\n"
        + "state_roundtrip: result=pass requested_at=11.800000 t=11.800000\n"
        + restored + "\n"
        + "state_replay: phase=restored event=end t=12.800000\n"
        + suffix)


class PagingStateRoundtripTraceCheckTest(unittest.TestCase):
    def test_accepts_identical_paging_replay(self):
        check(replay_log())

    def test_rejects_divergent_replay(self):
        with self.assertRaisesRegex(ValueError, "diverged"):
            check(replay_log(RECORD.replace("sequence=1", "sequence=2")))

    def test_rejects_failed_roundtrip(self):
        with self.assertRaisesRegex(ValueError, "successful"):
            check(replay_log().replace("result=pass", "result=fail"))


if __name__ == "__main__":
    unittest.main()
