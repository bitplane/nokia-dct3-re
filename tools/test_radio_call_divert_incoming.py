import unittest

from tools.radio_call_divert_incoming_trace_check import verify
from tools.run_host_diverted_call_gate import validate_phases


class CallDivertIncomingTest(unittest.TestCase):
    def test_forwarded_path(self):
        validate_phases(["queued", "forwarded"])
        verify("GSM incoming call forwarded before paging destination_length=5\n"
               "phase=forwarded reason=unconditional destination_length=5\n")

    def test_handset_phase_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "unexpected"):
            validate_phases(["queued", "paging"])
        with self.assertRaisesRegex(ValueError, "reached handset"):
            verify("GSM incoming call forwarded before paging destination_length=5\n"
                   "phase=forwarded reason=unconditional destination_length=5\n"
                   "incoming-call phase=alerting\n")

    def test_missing_decision_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "missing"):
            verify("ordinary boot\n")


if __name__ == "__main__":
    unittest.main()
