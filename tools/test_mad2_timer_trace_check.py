import unittest

from tools.mad2_timer_trace_check import check, parse


GOOD_TRACE = """
mad2_timer: event=W off=0f data=f9 old=31 pc=1 t=0.100000
mad2_timer: event=R off=10 data=12 pc=1 t=0.100001
mad2_timer: event=R off=11 data=34 pc=1 t=0.100001
mad2_timer: event=W off=12 data=12 old=00 pc=1 t=0.100002
mad2_timer: event=W off=13 data=36 old=00 pc=1 t=0.100002
mad2_timer: event=assert counter=1236 compare=1236 divider=f9 pending=010 mask=ff ctrl=0a t=0.100003
mad2_timer: event=W off=0a data=ef old=ff pc=1 t=0.100004
mad2_timer: event=ack mask=010 pending_before=010 pending_after=000 t=0.100005
"""


class Mad2TimerTraceCheckTest(unittest.TestCase):
    def test_complete_timer_lifecycle(self):
        errors, counts = check(
            parse(GOOD_TRACE), "final_fiq_status=00\n", expected_line=4
        )
        self.assertEqual([], errors)
        self.assertEqual(1, counts["clearing_acks"])

    def test_rejects_wrong_line(self):
        trace = GOOD_TRACE.replace("pending=010", "pending=004").replace(
            "mask=010 pending_before=010", "mask=004 pending_before=004"
        )
        errors, _ = check(parse(trace), "final_fiq_status=00\n", expected_line=4)
        self.assertIn("timer assertion did not use expected FIQ line 4", errors)

    def test_rejects_masked_lifecycle(self):
        trace = GOOD_TRACE.replace(
            "mad2_timer: event=W off=0a data=ef old=ff pc=1 t=0.100004\n", ""
        )
        errors, _ = check(parse(trace), "final_fiq_status=00\n", expected_line=4)
        self.assertIn("FIQ line 4 was never unmasked", errors)

    def test_accepts_unmasked_assertion_without_mask_write(self):
        trace = GOOD_TRACE.replace(
            "mad2_timer: event=W off=0a data=ef old=ff pc=1 t=0.100004\n", ""
        ).replace("pending=010 mask=ff", "pending=010 mask=ef")
        errors, _ = check(parse(trace), "final_fiq_status=00\n", expected_line=4)
        self.assertEqual([], errors)

    def test_rejects_missing_ack(self):
        trace = GOOD_TRACE.replace(
            "mad2_timer: event=ack mask=010 pending_before=010 pending_after=000 t=0.100005\n",
            "",
        )
        errors, _ = check(parse(trace), "final_fiq_status=10\n", expected_line=4)
        self.assertIn("FIQ line 4 was not acknowledged and cleared", errors)

    def test_rejects_torn_counter_read(self):
        trace = GOOD_TRACE.replace(
            "mad2_timer: event=R off=11 data=34 pc=1 t=0.100001",
            "mad2_timer: event=R off=11 data=34 pc=1 t=0.101000",
        )
        errors, _ = check(parse(trace), "final_fiq_status=00\n", expected_line=4)
        self.assertIn("no coherent timer counter MSB/LSB read found", errors)


if __name__ == "__main__":
    unittest.main()
