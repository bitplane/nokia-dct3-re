import unittest

from tools.alarm_trace_check import check_trace


class AlarmTraceCheckTest(unittest.TestCase):
    def test_accepts_ordered_organic_alarm(self):
        trace = "\n".join(
            (
                "ccont_rtc: event=alarm_write reg=0b data=02 armed=1 t=43.70",
                "ccont_rtc: event=alarm_write reg=0c data=0c armed=1 t=43.71",
                "ccont_rtc: event=second time=12:02:00 day=0 status=b3 mask=50 t=121.0",
                "rtc_alarm_route: map-output=06bc base=0 offset=0 deadline=7fffffff task=05 t=126.9",
                "buzzer: enabled=1 divider=4864 frequency=2672 volume=7 t=127.0",
            )
        )
        self.assertEqual([], check_trace(trace))

    def test_rejects_buzzer_before_alarm_consumption(self):
        trace = "\n".join(
            (
                "ccont_rtc: event=alarm_write reg=0b data=02 armed=1 t=43.70",
                "ccont_rtc: event=alarm_write reg=0c data=0c armed=1 t=43.71",
                "buzzer: enabled=1 divider=4864 frequency=2672 volume=7 t=100.0",
                "ccont_rtc: event=second time=12:02:00 day=0 status=b3 mask=50 t=121.0",
                "rtc_alarm_route: map-output=06bc deadline=7fffffff task=05 t=126.9",
            )
        )
        self.assertIn("organic alarm handling did not start the MAD2 buzzer", check_trace(trace))

    def test_rejects_unlatched_alarm_source(self):
        trace = "\n".join(
            (
                "ccont_rtc: event=alarm_write reg=0b data=02 armed=1 t=43.70",
                "ccont_rtc: event=alarm_write reg=0c data=0c armed=1 t=43.71",
                "ccont_rtc: event=second time=12:02:00 day=0 status=33 mask=50 t=121.0",
            )
        )
        self.assertIn("CCONT did not latch alarm source bit 7 at 12:02", check_trace(trace))
