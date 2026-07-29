import unittest

from tools.radio_outgoing_host_hostile_trace_check import check


class HostHostileTraceCheckTests(unittest.TestCase):
    def test_accepts_bounded_pre_setup_rejection(self):
        check(
            "gsm_call_adapter: termination id=1 cause=16 result=rejected\n"
            "gsm_call_adapter: queue overflow dropped=48\n"
            "GSM outgoing request id=1 digits=5551234\n"
            "gsm_call_adapter: decision id=1 outcome=1 result=accepted\n"
        )

    def test_rejects_clear_from_hostile_input(self):
        with self.assertRaisesRegex(ValueError, "initiated call clearing"):
            check(
                "gsm_call_adapter: termination id=1 cause=16 result=rejected\n"
                "gsm_call_adapter: queue overflow dropped=1\n"
                "GSM outgoing request id=1 digits=5551234\n"
                "gsm_call_adapter: decision id=1 outcome=1 result=accepted\n"
                "gsm_network: downlink CC Disconnect\n"
            )


if __name__ == "__main__":
    unittest.main()
