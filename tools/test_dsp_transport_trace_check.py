import unittest

from tools.dsp_transport_trace_check import check, check_bootstrap_completion


FULL = """
dspif_transport: peer RAM W off=000 data=0001
dspif_transport: peer RAM W off=002 data=0001
dspif_transport: peer RAM W off=004 data=0001
dspif_transport: peer RAM W off=0e0 data=0000
dspif_transport: peer RAM W off=0fe data=0001
dspif_transport: peer RAM W off=100 data=0001
dspif_transport: RAM W off=0fe data=0000
dspif_transport: RAM W off=100 data=0000
dsp_hle: bootstrap completion exchanges=64 publications=3
dspif_transport: doorbell command=0004 pending=0000
dspif_transport: RAM W off=0e4 data=0002
dspif_transport: IRQ4 service-complete
dsp_hle: TX packet type=70 payload=2 words=2
dspif_transport: TX consume type=70 payload=2 words=2 consumer=02
dspif_transport: RX enqueue type=74 payload=2 producer=082
dspif_transport: FIQ0 notify producer=082 consumer=080
external_service: response command=64 result=01 sequence=42
"""


class DspTransportTraceCheckTest(unittest.TestCase):
    def test_full_contract(self):
        self.assertEqual(check(FULL, True)["tx_packets"], 1)

    def test_bootstrap_contract(self):
        result = check(FULL, False, expected_bootstrap_exchanges=64)
        self.assertEqual(result["bootstrap_exchanges"], 64)

    def test_wrong_bootstrap_count_fails(self):
        with self.assertRaisesRegex(ValueError, "expected 58"):
            check(FULL, False, expected_bootstrap_exchanges=58)

    def test_completion_only_counts_both_exchange_words(self):
        requests = "".join(
            "dspif_transport: RAM W off=0fe data=0000\n"
            "dspif_transport: RAM W off=100 data=0000\n"
            for _ in range(58)
        )
        text = requests + (
            "dsp_hle: bootstrap completion exchanges=58 publications=3\n"
        )
        self.assertEqual(
            check_bootstrap_completion(text, 58), {"bootstrap_exchanges": 58})

    def test_completion_only_rejects_an_incomplete_pair(self):
        text = (
            "dspif_transport: RAM W off=0fe data=0000\n"
            "dsp_hle: bootstrap completion exchanges=1 publications=3\n"
        )
        with self.assertRaisesRegex(ValueError, "word 100"):
            check_bootstrap_completion(text, 1)

    def test_missing_consume_fails(self):
        with self.assertRaisesRegex(ValueError, "consumer advance"):
            check(FULL.replace("dspif_transport: TX consume", "missing"), True)

    def test_conformance_contract(self):
        self.assertEqual(check("dspif_fixture: conformance=07", False, True), {"conformance": 7})


if __name__ == "__main__":
    unittest.main()
