import unittest

from tools.run_noki6250_alarm import (
    check_alarm, check_restore, check_cold_alarm, check_alarm_seed, check_awake_restore)


class AlarmCheckTest(unittest.TestCase):
    def awake_restore_trace(self):
        cpu = ','.join(['00000000'] * 37)
        def state(event, time):
            return (f'6250_alarm_awake_state: event={event} pc=00200000 sp=00100000 '
                    f'ram=12345678 cpu={cpu} t={time}\n')
        tick = 'ccont_rtc: event=second time=13:48:06 day=0 status=81 mask=10\n'
        tick += 'buzzer: enabled=1 divider=5202 frequency=2499 volume=5 t=65.1\n'
        return ('ccont_rtc: event=second time=13:48:00 day=0 status=b1 mask=30\n'
                'ccont_rtc: event=status_ack data=81 old=b1\n' +
                state('saved', '65.000000000') + tick +
                state('reference', '66.250000000') +
                state('restored', '65.000000000') + tick +
                state('replayed', '66.250000000') +
                '6250_alarm_awake_restore: PASS\n')

    def test_awake_alarm_restore(self):
        self.assertIn('event=restored', check_awake_restore(self.awake_restore_trace()))

    def test_awake_alarm_restore_rejects_incomplete_or_changed_state(self):
        text = self.awake_restore_trace()
        for old, new in (('event=restored', 'event=missing'),
                         ('event=replayed pc=00200000', 'event=replayed pc=00200004'),
                         ('event=saved pc=', 'event=saved bad='),
                         ('event=reference pc=', 'event=reference pc=xx'),
                         ('6250_alarm_awake_restore: PASS', ''),
                         ('data=81 old=b1', 'data=00 old=b1'),
                         ('volume=5', 'volume=0'),
                         ('66.250000000', '66.500000000')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_awake_restore(text.replace(old, new))

    def test_awake_alarm_restore_rejects_off_or_early_stop(self):
        for prefix in ('ccont_power: event=off\n', '6250_alarm_physical: action=stop\n'):
            with self.assertRaises(ValueError):
                check_awake_restore(prefix + self.awake_restore_trace())

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

    def seed_trace(self):
        return self.trace().split("ccont_rtc: event=second", 1)[0]

    def test_pending_seed(self):
        check_alarm_seed(self.seed_trace())

    def test_seed_wrong_route_and_expired_deadline(self):
        for text in (self.seed_trace().replace("action=clock", "action=settings", 1),
                     self.seed_trace() + "ccont_rtc: event=second time=13:48:00\n",
                     self.seed_trace() + "6250_alarm_physical: event=stopped_presented\n",
                     self.seed_trace().replace("action=confirm", "action=idle", 1)):
            with self.assertRaises(ValueError):
                check_alarm_seed(text)

    def test_seed_programming_before_confirmation_rejected(self):
        text = self.seed_trace()
        confirm = "6250_alarm_physical: action=confirm\n"
        text = text.replace(confirm, "").replace("6250_alarm_physical: action=idle", confirm + "6250_alarm_physical: action=idle")
        with self.assertRaises(ValueError):
            check_alarm_seed(text)

    def cold_trace(self):
        return "\n".join((
            "ccont_rtc: event=alarm_write reg=0b data=30 armed=1",
            "ccont_rtc: event=alarm_write reg=0c data=0d armed=1",
            "6250_alarm_cold: event=armed_observed",
            "ccont_rtc: event=second time=13:48:00 day=0 status=b1 mask=30",
            "ccont_rtc: event=status_ack data=81 old=b1",
            "buzzer: enabled=1 divider=5202 frequency=2499 volume=5",
            "6250_alarm_cold: action=stop",
            "buzzer: enabled=0 divider=0 frequency=0",
            "6250_alarm_cold: event=stopped_observed")) + "\n"

    def test_cold_reconstructed_alarm(self):
        check_cold_alarm(self.cold_trace())

    def test_cold_requires_deadline_reconstruction_and_ack(self):
        for old, new in (("data=30", "data=31"), ("13:48:00", "13:49:00"),
                         ("data=81", "data=01")):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_cold_alarm(self.cold_trace().replace(old, new))

    def test_cold_rejects_arming_input_and_resumed_buzzer(self):
        for suffix in ("6250_alarm_physical: action=confirm\n",
                       "6250_alarm_cold: action=stop\n",
                       "buzzer: enabled=1 divider=5202 frequency=2499 volume=5\n"):
            with self.assertRaises(ValueError):
                check_cold_alarm(self.cold_trace() + suffix)

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

    def off_snooze_trace(self, choice):
        text = self.snooze_trace().replace(
            "ccont_rtc: event=second time=13:48:00",
            "6250_alarm_physical: action=power_off\n"
            "ccont_power: event=off\n6250_alarm_physical: action=power_release\n"
            "ccont_power: event=wake cause=80\nccont_rtc: event=second time=13:48:00", 1)
        text = text.replace(
            "status=b1 mask=10", "status=b1 mask=50").replace(
            "ccont_rtc: event=second time=13:53:00",
            "ccont_power: event=off\nccont_power: event=wake cause=80\n"
            "ccont_rtc: event=second time=13:53:00", 1)
        text += "6250_alarm_physical: action=activate_" + choice + "\n"
        if choice == "no":
            text += "ccont_power: event=off\n"
        return text

    def test_powered_off_snooze_choices(self):
        for choice in ("yes", "no"):
            check_alarm(self.off_snooze_trace(choice), choice, snooze=True)

    def test_powered_off_snooze_missing_wake_and_wrong_mask(self):
        text = self.off_snooze_trace("no")
        for broken in (text.replace("ccont_power: event=wake cause=80\n", "", 1),
                       text.replace("mask=50", "mask=10"),
                       text.replace("cause=80", "cause=02")):
            with self.assertRaises(ValueError):
                check_alarm(broken, "no", snooze=True)

    def test_powered_off_snooze_requires_endpoint_silence(self):
        text = self.off_snooze_trace("no").replace(
            "ccont_power: event=off\nccont_power: event=wake",
            "ccont_power: event=off\ndspif_transport: peer RAM W\n"
            "ccont_power: event=wake", 1)
        with self.assertRaises(ValueError):
            check_alarm(text, "no", snooze=True)

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

    def restore_trace(self):
        registers = ",".join(["00000000"] * 37)
        state = " pc=00000000 sp=00000000 ram=12345678 cpu=" + registers + " t=49.000000000\n"
        tick = "ccont_rtc: event=second time=13:47:50 day=0 status=31 mask=10 t=50.000000000\n"
        return ("6250_alarm_state: event=saved" + state +
                "6250_alarm_replay: phase=reference event=begin t=49.000000000\n" + tick +
                "6250_alarm_replay: phase=reference event=end t=50.250000000\n" +
                "6250_alarm_state: event=restored" + state +
                "6250_alarm_replay: phase=restored event=begin t=49.000000000\n" + tick +
                "6250_alarm_replay: phase=restored event=end t=50.250000000\n")

    def test_exact_restore_and_rtc_replay(self):
        result = check_restore(self.restore_trace())
        self.assertNotIn("event=saved", result)
        self.assertIn("event=restored", result)
        self.assertEqual(result.count("event=second"), 1)

    def test_snooze_countdown_checkpoint(self):
        text = self.restore_trace().replace("t=49.000000000", "t=100.000000000").replace(
            "t=50.250000000", "t=101.250000000")
        check_restore(text, checkpoint=100)
        with self.assertRaises(ValueError):
            check_restore(text)
        with self.assertRaises(ValueError):
            check_restore(text.replace("t=101.250000000", "t=101.500000000", 1),
                          checkpoint=100)

    def test_restore_architecture_time_and_tick_mismatch_rejected(self):
        for old, new in (("ram=12345678", "ram=12345679"),
                         ("t=50.250000000", "t=50.500000000"),
                         ("time=13:47:50", "time=13:47:51")):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_restore(self.restore_trace().replace(old, new, 1))

    def test_incomplete_registers_and_early_wake_rejected(self):
        with self.assertRaises(ValueError):
            check_restore(self.restore_trace().replace(",00000000", "", 1))
        with self.assertRaises(ValueError):
            check_restore(self.restore_trace().replace(
                "6250_alarm_replay: phase=reference event=end",
                "ccont_power: event=wake cause=80\n6250_alarm_replay: phase=reference event=end"))

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
