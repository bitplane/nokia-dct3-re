import unittest

from tools.mad2_timer1_trace_check import check


VALID = """
mad2_timer1: event=destination counter=7fff destination=7fff pending=020 t=1.0
mad2_timer: event=ack mask=020 pending_before=020 pending_after=000 t=1.1
"""


class Mad2Timer1TraceCheckTest(unittest.TestCase):
    def test_valid_contract(self):
        errors, counts = check(VALID)
        self.assertEqual([], errors)
        self.assertEqual(1, counts["destinations"])
        self.assertEqual(1, counts["fiq5_acknowledgements"])

    def test_requires_fiq5_ack(self):
        errors, _ = check(VALID.splitlines()[1])
        self.assertIn("firmware did not acknowledge Timer-1 FIQ5/status bit 0x020", errors)

    def test_rejects_wrong_destination(self):
        errors, _ = check(VALID.replace("counter=7fff destination=7fff", "counter=ffff destination=ffff"))
        self.assertIn("Timer-1 destination event did not occur at 0x7fff", errors)


if __name__ == "__main__":
    unittest.main()
