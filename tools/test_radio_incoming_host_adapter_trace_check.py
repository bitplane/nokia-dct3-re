import unittest

from tools.radio_incoming_host_adapter_trace_check import check


TRACE = """
gsm_call_adapter: incoming id=1 result=accepted
gsm_call_adapter: incoming state id=1 epoch=1 phase=queued
gsm_call_adapter: incoming state id=1 epoch=1 phase=paging
gsm_call_adapter: incoming state id=1 epoch=1 phase=alerting
gsm_call_adapter: incoming state id=1 epoch=1 phase=connected
gsm_call_adapter: media direction=uplink id=1 sequence=0 good=1
gsm_call_adapter: media direction=downlink id=1 sequence=0 result=accepted
gsm_call_adapter: termination id=1 cause=16 result=accepted
gsm_call_adapter: incoming state id=1 epoch=1 phase=ended
"""


class IncomingHostAdapterTraceCheckTest(unittest.TestCase):
    def test_complete_lifecycle(self):
        check(TRACE)

    def test_restore_requires_second_connected_epoch(self):
        with self.assertRaises(ValueError):
            check(TRACE, True)
        check(TRACE.replace(
            "gsm_call_adapter: termination",
            "gsm_call_adapter: incoming state id=1 epoch=2 phase=connected\n"
            "gsm_call_adapter: termination"), True)


if __name__ == "__main__":
    unittest.main()
