import unittest

from tools.radio_a5_trace_check import check


GOOD = """
dspif_transport: RX enqueue type=80 payload=34 producer=0d9 data=801200000635012b
dsp_hle: TX packet type=14 payload=12 words=7 radio_phase=service_uplink_request data=<redacted>
dsp_hle: GSM service uplink sapi=0 pd=06 message=32 length=2 data=0632
gsm_cipher: event=activated algorithm=1 t=20.2
radio_l1: kind=cipher direction=uplink algorithm=1 fn=4378 count=6378
radio_l1: kind=cipher direction=downlink algorithm=1 fn=4378 count=6378
radio_l1: kind=xcch direction=uplink algorithm=1 first_fn=4335 last_fn=4338
radio_l1: kind=xcch direction=downlink algorithm=1 first_fn=4386 last_fn=4389
"""


class A5TraceCheckTest(unittest.TestCase):
    def test_accepts_semantic_trace(self):
        self.assertEqual(check(GOOD), [])

    def test_rejects_missing_direction(self):
        self.assertTrue(check(GOOD.replace(
            "radio_l1: kind=cipher direction=downlink", "other")))

    def test_rejects_secret(self):
        self.assertTrue(check(GOOD + """
dsp_hle: TX packet type=14 payload=12 data=020001020304050607080000
"""))

    def test_rejects_premature_bursts(self):
        lines = GOOD.splitlines()
        moved = "\n".join(lines[:1] + lines[4:5] + lines[1:4] + lines[5:])
        self.assertTrue(check(moved))


if __name__ == "__main__":
    unittest.main()
