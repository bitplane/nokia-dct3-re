import unittest

from tools.mad2_clock_trace_check import check, parse


VALID = """
mad2_clock: event=R off=01 data=01 counter=0001 pc=0023060c t=0.1
mad2_clock: event=W off=0d data=0c old=00 counter=0006 pc=002b6442 t=0.2
mad2_clock: event=W off=03 data=31 old=ff counter=0010 pc=002b4dca t=0.3
mad2_clock: event=W off=0d data=2c old=0c counter=0020 pc=002a0616 t=0.4
mad2_clock: event=W off=01 data=05 old=01 counter=0021 pc=002b4e12 t=0.5
mad2_clock: event=W off=0d data=0c old=2c counter=0022 pc=002a0106 t=0.6
"""


class Mad2ClockTraceCheckTest(unittest.TestCase):
    def test_valid_boot_contract(self):
        errors, counts = check(parse(VALID))
        self.assertEqual([], errors)
        self.assertEqual(0, counts["timer1_accesses"])
        self.assertTrue(counts["sim_clock_lifecycle"])

    def test_requires_sim_clock_gate_close(self):
        events = parse(VALID.replace(
            "mad2_clock: event=W off=0d data=0c old=2c counter=0022 pc=002a0106 t=0.6\n", ""))
        errors, _ = check(events)
        self.assertIn(
            "SIM clock gate did not complete the observed 0x0c -> 0x2c -> 0x0c lifecycle",
            errors,
        )

    def test_rejects_new_timer1_access(self):
        events = parse(VALID + "mad2_clock: event=R off=04 data=00 counter=0022 pc=00200000 t=0.6\n")
        errors, _ = check(events)
        self.assertIn("Timer1 offsets 0x04..0x07 unexpectedly became active in the boot contract", errors)

    def test_reset_write_is_optional(self):
        errors, _ = check(parse(VALID.replace(
            "mad2_clock: event=W off=01 data=05 old=01 counter=0021 pc=002b4e12 t=0.5\n", "")))
        self.assertEqual([], errors)

    def test_mad2_watchdog_service_is_conditional(self):
        without_watchdog = "\n".join(
            line for line in VALID.splitlines() if "off=03" not in line)
        errors, counts = check(parse(without_watchdog))
        self.assertEqual([], errors)
        self.assertEqual(0, counts["watchdog_writes"])

    def test_rejects_unknown_watchdog_service_value(self):
        errors, _ = check(parse(VALID.replace("off=03 data=31", "off=03 data=30")))
        self.assertIn("MAD2 watchdog service used a value other than 0x31", errors)

    def test_rejects_unknown_reset_write(self):
        errors, _ = check(parse(VALID.replace("data=05 old=01", "data=04 old=01")))
        self.assertIn(
            "reset-control write differed from the observed 0x01 -> 0x05 lifecycle transition",
            errors,
        )

    def test_requires_post_request_reset_cause(self):
        reset = VALID + "mad2_clock: event=R off=01 data=05 counter=0000 pc=0023060c t=0.7\n"
        errors, counts = check(parse(reset), require_software_reset=True)
        self.assertEqual([], errors)
        self.assertTrue(counts["software_reset_completed"])

    def test_rejects_request_without_completed_reset(self):
        errors, _ = check(parse(VALID), require_software_reset=True)
        self.assertIn("MCU reset request was not followed by reset-cause value 0x05", errors)

    def test_requires_watchdog_reset_cause(self):
        watchdog = VALID.replace("off=03 data=31 old=ff", "off=03 data=01 old=ff")
        watchdog += "mad2_clock: event=R off=01 data=03 counter=0000 pc=0023060c t=1.0\n"
        errors, counts = check(
            parse(watchdog), require_watchdog_reset=True, require_watchdog=False)
        self.assertEqual([], errors)
        self.assertTrue(counts["watchdog_reset_completed"])

    def test_rejects_watchdog_without_completed_reset(self):
        watchdog = VALID.replace("off=03 data=31 old=ff", "off=03 data=01 old=ff")
        errors, _ = check(
            parse(watchdog), require_watchdog_reset=True, require_watchdog=False)
        self.assertIn("MAD2 watchdog expiry was not followed by reset-cause value 0x03", errors)


if __name__ == "__main__":
    unittest.main()
