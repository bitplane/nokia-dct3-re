import unittest

from tools.mad2_clock_trace_check import check, parse


VALID = """
mad2_clock: event=R off=01 data=01 counter=0001 pc=0023060c t=0.1
mad2_clock: event=W off=0d data=0c old=00 counter=0006 pc=002b6442 t=0.2
mad2_clock: event=W off=03 data=31 old=ff counter=0010 pc=002b4dca t=0.3
mad2_clock: event=W off=0d data=2c old=0c counter=0020 pc=002a0616 t=0.4
mad2_clock: event=W off=01 data=05 old=01 counter=0021 pc=002b4e12 t=0.5
"""


class Mad2ClockTraceCheckTest(unittest.TestCase):
    def test_valid_boot_contract(self):
        errors, counts = check(parse(VALID))
        self.assertEqual([], errors)
        self.assertEqual(0, counts["timer1_accesses"])

    def test_rejects_new_timer1_access(self):
        events = parse(VALID + "mad2_clock: event=R off=04 data=00 counter=0022 pc=00200000 t=0.6\n")
        errors, _ = check(events)
        self.assertIn("Timer1 offsets 0x04..0x07 unexpectedly became active in the boot contract", errors)


if __name__ == "__main__":
    unittest.main()
