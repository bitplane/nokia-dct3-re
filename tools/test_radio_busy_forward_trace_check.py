import unittest

from tools.radio_busy_forward_trace_check import verify


GOOD = """
gsm_ss: request=register transaction=1b invoke=1 service=29 number_length=5 active=1 t=1
gsm_call_adapter: state id=1 epoch=1 phase=connected t=2
dsp_hle: GSM incoming call forwarded before paging destination_length=5 condition=busy t=3
gsm_call_adapter: incoming state id=2 epoch=1 phase=forwarded reason=busy destination_length=5 t=4
"""


class RadioBusyForwardTraceCheckTest(unittest.TestCase):
    def test_accepts_busy_forwarding(self):
        verify(GOOD)

    def test_requires_active_call_first(self):
        with self.assertRaisesRegex(ValueError, "event 2"):
            verify(GOOD.replace("phase=connected", "phase=alerting"))

    def test_rejects_handset_paging(self):
        with self.assertRaisesRegex(ValueError, "reached handset"):
            verify(GOOD + "GSM incoming page\n")


if __name__ == "__main__":
    unittest.main()
