import unittest
from mad2_wake_report_check import check


def fixture(product="noki3310", configured=1):
    phases = ["awake_edge", "armed", "wake_edge", "ack_report", "blocked_request", "complete"]
    pending = [1, 0, 9 if configured else 1, 1, 1, 0]
    return "\n".join(
        f"mad2_wake_report_fixture: phase={phase} machine={product} configured={configured} "
        f"irq={irq:02x} ctrl=0a t={0.2 if i < 2 else 0.25:.9f} mmio_conformance=1"
        for i, (phase, irq) in enumerate(zip(phases, pending)))


class WakeReportTests(unittest.TestCase):
    def test_products(self):
        for product, flag in (("noki3310", 1), ("noki3330", 1), ("noki3210", 0)):
            self.assertEqual(check(fixture(product, flag), product), [])

    def test_missing_report(self):
        self.assertTrue(check(fixture().replace("irq=09", "irq=01"), "noki3310"))

    def test_gate_override(self):
        self.assertTrue(check(fixture().replace("ctrl=0a", "ctrl=05"), "noki3310"))

    def test_missing_phase(self):
        self.assertTrue(check("\n".join(fixture().splitlines()[:-1]), "noki3310"))

    def test_wrong_scope(self):
        self.assertTrue(check(fixture(), "noki3210"))

    def test_no_sleep_interval(self):
        self.assertTrue(check(fixture().replace("0.250000000", "0.200000000"), "noki3310"))


if __name__ == "__main__":
    unittest.main()
