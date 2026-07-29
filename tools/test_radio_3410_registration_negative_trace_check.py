import unittest

from tools.radio_3410_registration_negative_trace_check import verify


BARRED = "RX enqueue type=80 payload=34 data=005049061b000100f110000140000000000002\n"
RXLEV = "RX enqueue type=80 payload=34 data=005049061b000100f110000140000000003f\n"
ASSIGNMENT = """
TX packet type=0c payload=6 radio_phase=serving_bcch data=00000f660000
RX enqueue type=80 payload=34 data=601200000001000100002d063f002000010e182b
"""


class Radio3410RegistrationNegativeTraceCheckTest(unittest.TestCase):
    def test_accepts_unsuitable_cells(self):
        verify(BARRED, "barred")
        verify(RXLEV, "rxlev")

    def test_accepts_mismatched_assignment(self):
        verify(ASSIGNMENT, "assignment")

    def test_rejects_access_on_unsuitable_cell(self):
        with self.assertRaisesRegex(ValueError, "CHANNEL REQUEST"):
            verify(BARRED + "TX packet type=0c data=00000f\n", "barred")

    def test_rejects_matching_assignment(self):
        with self.assertRaisesRegex(ValueError, "unexpectedly matched"):
            verify(ASSIGNMENT.replace("010e18", "010f18"), "assignment")

    def test_rejects_subscriber_mutation(self):
        with self.assertRaisesRegex(ValueError, "EF_LOCI"):
            verify(
                ASSIGNMENT + "sim_device: update-binary fid=6f7e offset=4\n",
                "assignment")


if __name__ == "__main__":
    unittest.main()
