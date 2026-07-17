import unittest

from tools.charger_lifecycle_check import check, parse_summary


class ChargerLifecycleCheckTest(unittest.TestCase):
    def test_connected_startup(self):
        values = parse_summary(
            "startup_modes=0001,0009,000D\nfinal_startup_mode=0009\n"
            "final_sim_enable=01\nirq_seen=04\nlcd_data_writes=10\n"
        )
        self.assertEqual([], check("connected", values))

    def test_acting_dead(self):
        values = parse_summary(
            "startup_modes=0001,0005,0009,000C,000D\nfinal_startup_mode=0005\n"
            "final_sim_enable=00\nirq_seen=04\nlcd_data_writes=10\n"
        )
        self.assertEqual([], check("acting-dead", values))

    def test_rejects_ordinary_shutdown(self):
        values = parse_summary(
            "startup_modes=0001,0004,000C,000D\nfinal_startup_mode=000C\n"
            "final_sim_enable=00\nirq_seen=00\nlcd_data_writes=10\n"
        )
        errors = check("acting-dead", values)
        self.assertIn("acting-dead lifecycle never entered mode 0009", errors)
        self.assertIn("acting-dead lifecycle never entered mode 0005", errors)


if __name__ == "__main__":
    unittest.main()
