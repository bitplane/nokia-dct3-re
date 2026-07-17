import unittest

from tools.charger_wake_check import check, parse_summary


class ChargerWakeCheckTest(unittest.TestCase):
    def test_complete_wake_lifecycle(self):
        log = """
ccont_power: event=off t=8.0
ccont_power: event=wake cause=04 t=13.0
ccont_power: event=cause_read data=0d t=13.1
ccont_input: adc_select=5 raw=3ff ctrl=58 t=13.2
"""
        summary = parse_summary(
            "startup_modes=0001,0004,0005,000C,000D\n"
            "final_startup_mode=0005\nfinal_sim_enable=00\nlcd_data_writes=10\n"
        )
        self.assertEqual([], check(log, summary))

    def test_rejects_interrupt_only_without_restart(self):
        log = "ccont_input: adc_select=5 raw=3ff ctrl=58 t=13.2\n"
        summary = parse_summary(
            "startup_modes=0001,0004,000C,000D\n"
            "final_startup_mode=000C\nfinal_sim_enable=00\nlcd_data_writes=10\n"
        )
        errors = check(log, summary)
        self.assertIn("CCONT never removed baseband power", errors)
        self.assertIn("charger edge never restored CCONT baseband power with cause 04", errors)


if __name__ == "__main__":
    unittest.main()
