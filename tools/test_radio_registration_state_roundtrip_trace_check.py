import unittest

from tools.radio_registration_state_roundtrip_trace_check import check
from tools.test_radio_registration_trace_check import NHM6_GOOD


def replay_log(reference: str, restored: str | None = None) -> str:
    if restored is None:
        restored = reference
    return (
        "state_replay: phase=reference event=begin t=10.450000\n"
        + reference
        + "\nstate_replay: phase=reference event=end t=11.450000\n"
        + "state_roundtrip: result=pass timer_delta=0000 mode=1234 "
          "requested_at=10.450000 t=10.450000\n"
        + "state_replay: phase=restored event=begin t=10.450000\n"
        + restored
        + "\nstate_replay: phase=restored event=end t=11.450000\n"
    )


class RegistrationStateRoundtripTraceCheckTest(unittest.TestCase):
    def test_accepts_identical_registration_replay(self):
        check(replay_log(NHM6_GOOD))

    def test_rejects_divergent_replay(self):
        with self.assertRaisesRegex(ValueError, "diverged"):
            check(replay_log(NHM6_GOOD, NHM6_GOOD.replace(
                "data=0080034101", "data=0080032101")))

    def test_rejects_missing_roundtrip_result(self):
        with self.assertRaisesRegex(ValueError, "save/load"):
            check(replay_log(NHM6_GOOD).replace(
                "state_roundtrip: result=pass", "state_roundtrip: result=fail"))


if __name__ == "__main__":
    unittest.main()
