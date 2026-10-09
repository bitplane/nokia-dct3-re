import unittest

from tools.run_noki6250_alarm import check_alarm


class AlarmCheckTest(unittest.TestCase):
    def trace(self):
        actions = ["menu", *[f"down_{i}" for i in range(1, 10)], "clock",
                   "clock_down_1", "alarm", *[f"time_{i}" for i in range(1, 5)],
                   "confirm", "idle", "stop"]
        lines = []
        for action in actions:
            lines.append("6250_alarm_physical: action=" + action)
            if action == "confirm":
                lines.extend(("ccont_rtc: event=alarm_write reg=0b data=30 armed=1",
                              "ccont_rtc: event=alarm_write reg=0c data=0d armed=1"))
            if action == "idle":
                lines.extend(("ccont_rtc: event=second time=13:48:00 day=0 status=b1 mask=30",
                              "ccont_rtc: event=status_ack data=81 old=b1",
                              "buzzer: enabled=1 divider=5202 frequency=2499 volume=5"))
            if action == "stop":
                lines.append("buzzer: enabled=0 divider=0 frequency=0 volume=5")
        lines.append("6250_alarm_physical: event=stopped_presented")
        return "\n".join(lines) + "\n"

    def test_own_alarm_contract(self):
        check_alarm(self.trace())

    def test_wrong_deadline_cause_or_ack_rejected(self):
        for old, new in (("13:48:00", "13:47:00"), ("status=b1", "status=31"),
                         ("data=81 old=b1", "data=01 old=b1"), ("data=30 armed=1", "data=00 armed=1")):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_alarm(self.trace().replace(old, new))

    def test_sibling_menu_route_rejected(self):
        with self.assertRaisesRegex(ValueError, "sequence"):
            check_alarm(self.trace().replace("action=clock_down_1", "action=settings"))

    def test_missing_output_or_stop_rejected(self):
        for line in ("buzzer: enabled=1 divider=5202 frequency=2499 volume=5\n",
                     "buzzer: enabled=0 divider=0 frequency=0 volume=5\n",
                     "6250_alarm_physical: event=stopped_presented\n"):
            with self.assertRaises(ValueError):
                check_alarm(self.trace().replace(line, ""))

    def test_resumed_buzzer_and_lua_error_rejected(self):
        for suffix in ("buzzer: enabled=1 divider=5202 frequency=2499 volume=5\n", "[LUA ERROR]\n"):
            with self.assertRaises(ValueError):
                check_alarm(self.trace() + suffix)


if __name__ == "__main__":
    unittest.main()
