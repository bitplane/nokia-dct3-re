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
    def signaling(self):
        return '\n'.join(line for line in GOOD.splitlines() if 'kind=cipher' not in line)

    def test_signaling_scope_does_not_promote_traffic(self):
        self.assertEqual(check(self.signaling(), require_traffic=False), [])
        self.assertTrue(check(self.signaling()))

    def test_signaling_requires_each_direction(self):
        for direction in ('uplink', 'downlink'):
            with self.subTest(direction=direction):
                self.assertTrue(check(self.signaling().replace(
                    f'kind=xcch direction={direction}', 'other'), require_traffic=False))

    def test_signaling_orders_key_control_and_ciphered_downlink(self):
        lines = self.signaling().strip().splitlines()
        for index in (1, 3):
            with self.subTest(index=index):
                moved = '\n'.join([lines[index]] + lines[:index] + lines[index+1:])
                self.assertTrue(check(moved, require_traffic=False))

    def test_accepts_semantic_trace(self):
        self.assertEqual(check(GOOD), [])

    def test_each_service_defers_mm_information_until_activation(self):
        command = 'GSM service downlink kind=7 sapi=0 pd=06 message=35 length=3\n'
        information = 'GSM service downlink kind=8 sapi=0 pd=05 message=32 length=10\n'
        activation = 'gsm_cipher: event=activated algorithm=1\n'
        self.assertEqual(check(GOOD + command + activation + information), [])
        for tail in (command + information + activation, command + information):
            with self.subTest(tail=tail):
                self.assertIn('MM Information precedes cipher completion', check(GOOD + tail))

    def test_accepts_decoded_cipher_command(self):
        semantic = GOOD.replace(
            "dspif_transport: RX enqueue type=80 payload=34 producer=0d9 "
            "data=801200000635012b",
            "dsp_hle: GSM service downlink kind=7 sapi=0 pd=06 message=35 length=3")
        self.assertEqual(check(semantic), [])

    def test_rejects_missing_direction(self):
        self.assertTrue(check(GOOD.replace(
            "radio_l1: kind=cipher direction=downlink", "other")))

    def test_rejects_secret(self):
        for stage in ('TX pending', 'TX packet', 'TX consume', 'DSP consume'):
            with self.subTest(stage=stage):
                text = GOOD + f'\ndspif_transport: {stage} type=14 payload=12 data=020001020304050607080000\n'
                self.assertTrue(check(text))
                self.assertTrue(check(text, require_traffic=False))

    def test_rejects_premature_bursts(self):
        lines = GOOD.splitlines()
        moved = "\n".join(lines[:1] + lines[4:5] + lines[1:4] + lines[5:])
        self.assertTrue(check(moved))


if __name__ == "__main__":
    unittest.main()
