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

    def power_trace(self, choice):
        text = self.trace().replace(
            "ccont_rtc: event=second", "6250_alarm_physical: action=power_off\n"
            "ccont_power: event=off\n6250_alarm_physical: action=power_release\n"
            "ccont_power: event=wake cause=80\nccont_rtc: event=second", 1)
        text += "6250_alarm_physical: action=activate_" + choice + "\n"
        if choice == "no":
            text += "ccont_power: event=off\n"
        return text

    def test_power_choices(self):
        for choice in ("yes", "no"):
            check_alarm(self.power_trace(choice), choice)

    def test_wrong_power_cause_and_missing_off_rejected(self):
        for text in (self.power_trace("no").replace("cause=80", "cause=02"),
                     self.power_trace("no").replace("ccont_power: event=off\n", "", 1),
                     self.power_trace("yes") + "ccont_power: event=off\n"):
            with self.assertRaises(ValueError):
                check_alarm(text, "no" if "activate_no" in text else "yes")

    def test_power_choice_must_match_physical_input(self):
        with self.assertRaises(ValueError):
            check_alarm(self.power_trace("no"), "yes")

    def snooze_trace(self):
        return self.trace().replace("6250_alarm_physical: action=stop", "\n".join((
            "6250_alarm_physical: action=snooze",
            "buzzer: enabled=0 divider=0 frequency=0",
            "ccont_rtc: event=alarm_write reg=0b data=35 armed=1",
            "ccont_rtc: event=alarm_write reg=0c data=0d armed=1",
            "6250_alarm_physical: event=snoozed_presented",
            "ccont_rtc: event=second time=13:53:00 day=0 status=b1 mask=10",
            "ccont_rtc: event=status_ack data=a1 old=b1",
            "buzzer: enabled=1 divider=5202 frequency=2499 volume=5",
            "6250_alarm_physical: event=recurrence_observed",
            "6250_alarm_physical: action=stop")))

    def test_snooze_recurrence(self):
        check_alarm(self.snooze_trace(), snooze=True)

    def test_snooze_wrong_deadline_ack_or_rearm_rejected(self):
        for old, new in (("13:53:00", "13:52:00"), ("data=a1", "data=21"),
                         ("data=35", "data=34"), ("event=recurrence_observed", "event=absent")):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_alarm(self.snooze_trace().replace(old, new), snooze=True)

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
