import unittest

from tools.dsp_transport_trace_check import check


FULL = """
dspif_transport: peer RAM W off=000 data=0001
dspif_transport: peer RAM W off=002 data=0001
dspif_transport: peer RAM W off=004 data=0001
dspif_transport: peer RAM W off=0e0 data=0000
dspif_transport: peer RAM W off=0fe data=0001
dspif_transport: peer RAM W off=100 data=0001
dspif_transport: RAM W off=0fe data=0000
dspif_transport: RAM W off=100 data=0000
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
        self.assertEqual(check(FULL, False)["doorbells"], 1)

    def test_missing_consume_fails(self):
        with self.assertRaisesRegex(ValueError, "consumer advance"):
            check(FULL.replace("dspif_transport: TX consume", "missing"), True)

    def test_conformance_contract(self):
        self.assertEqual(check("dspif_fixture: conformance=07", False, True), {"conformance": 7})


if __name__ == "__main__":
    unittest.main()
