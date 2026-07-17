import unittest

from tools.ccont_rtc_trace_check import check_trace


class CcontRtcTraceCheckTest(unittest.TestCase):
    def test_accepts_alarm_delivery(self):
        trace = "\n".join(
            (
                "ccont_rtc: event=alarm_write reg=0b data=01 armed=0",
                "ccont_rtc: event=alarm_write reg=0c data=0c armed=1",
                "ccont_route: state=1 irq_line=2 pending=004 mask=aa ctrl=05",
                "ccont_route: event=mad_ack data=04 pc=002af428",
                "ccont_rtc: event=second time=12:01:00 day=1 status=b3 mask=50",
            )
        )
        self.assertEqual([], check_trace(trace))

    def test_rejects_masked_route(self):
        trace = "\n".join(
            (
                "ccont_rtc: event=alarm_write reg=0b data=01",
                "ccont_rtc: event=alarm_write reg=0c data=0c",
                "ccont_route: state=1 irq_line=2 pending=004 mask=ae",
                "ccont_route: event=mad_ack data=04 pc=002af428",
                "ccont_rtc: event=second time=12:01:00 status=80",
            )
        )
        self.assertIn("MAD2 IRQ2 was masked during alarm delivery", check_trace(trace))

    def test_rejects_missing_mad_ack(self):
        trace = "\n".join(
            (
                "ccont_rtc: event=alarm_write reg=0b data=01",
                "ccont_rtc: event=alarm_write reg=0c data=0c",
                "ccont_route: state=1 irq_line=2 pending=004 mask=aa",
                "ccont_rtc: event=second time=12:01:00 status=80",
            )
        )
        self.assertIn("firmware did not acknowledge the MAD2 IRQ2 edge", check_trace(trace))


if __name__ == "__main__":
    unittest.main()
