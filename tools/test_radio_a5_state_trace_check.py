import unittest

from tools.radio_a5_state_trace_check import check


TRACE = """
state_replay: phase=reference event=begin t=1.000000
[:radio_peer] GSM service uplink sapi=0 pd=06 message=32 data=0632 t=1.010000
[:gsm_session] gsm_cipher: event=activated algorithm=1 t=1.010000
[:radio_peer] radio_l1: kind=xcch direction=downlink algorithm=1 first_fn=1 last_fn=4
state_replay: phase=reference event=end t=1.050000
state_replay: phase=restored event=begin t=1.000000
[:radio_peer] GSM service uplink sapi=0 pd=06 message=32 data=0632 t=1.010000
[:gsm_session] gsm_cipher: event=activated algorithm=1 t=1.010000
[:radio_peer] radio_l1: kind=xcch direction=downlink algorithm=1 first_fn=1 last_fn=4
state_replay: phase=restored event=end t=1.050000
state_roundtrip: result=pass
[:radio_peer] dsp_hle: PCH no-identity fill channel=60 fn=9 t=2.000000
"""


class A5StateTraceCheckTests(unittest.TestCase):
    def test_accepts_identical_transition(self):
        self.assertEqual([], check(TRACE))

    def test_rejects_divergent_frame(self):
        changed = TRACE.replace("first_fn=1 last_fn=4", "first_fn=2 last_fn=5", 1)
        self.assertIn(
            "restored cipher transition differs from reference", check(changed)
        )

    def test_requires_activation_inside_replay(self):
        self.assertIn(
            "replay interval did not contain cipher activation",
            check(TRACE.replace("gsm_cipher: event=activated", "gsm_cipher: event=x")),
        )


if __name__ == "__main__":
    unittest.main()
