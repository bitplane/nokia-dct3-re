import unittest

from tools.c54x_rom4_rf_consumption_check import check


def fixture():
    return "\n".join(
        f"rom4_rf_sample_consumption: index={i} t={1 + i / 1000000:.12f} "
        "pc=3310 counter=0010" for i in range(1, 65))


def scan_fixture():
    pcs = (0x330c, 0x3312, 0x3318, 0x331e)
    return "\n".join(
        f"rom4_rf_scan_consumption: index={i} t={1 + i / 1000000:.12f} "
        f"pc={pcs[(i - 1) % 4]:04x} counter=0010" for i in range(1, 65))


class ConsumptionTests(unittest.TestCase):
    def test_scan_window(self):
        text = scan_fixture()
        self.assertEqual(check(text, scan=True)["window"], "scan")

    def test_scan_sequence_order(self):
        with self.assertRaises(ValueError):
            check(scan_fixture().replace("pc=3312", "pc=3318", 1), scan=True)

    def test_scan_missing_loop_read(self):
        with self.assertRaises(ValueError):
            check(scan_fixture().replace("pc=331e", "pc=330c", 1), scan=True)

    def test_group_measurements(self):
        result = check(scan_fixture(), scan=True)
        self.assertAlmostEqual(result["group_start_interval_seconds"], 32e-6)
        self.assertAlmostEqual(result["between_groups_gap_seconds"], 1e-6)
        for span in result["group_spans_seconds"]:
            self.assertAlmostEqual(span, 31e-6)

    def test_startup_not_scan_evidence(self):
        with self.assertRaises(ValueError):
            check(fixture(), scan=True)

    def test_scan_pc_outside_filter(self):
        text = fixture().replace("sample_consumption", "scan_consumption").replace("pc=3310", "pc=324b")
        with self.assertRaises(ValueError):
            check(text, scan=True)

    def test_bounded_measurement_not_adc_rate(self):
        result = check(fixture())
        self.assertEqual(result["reads"], 64)
        self.assertAlmostEqual(result["span_seconds"], 63e-6)
        self.assertFalse(result["physical_sample_rate_proven"])

    def test_missing_read(self):
        with self.assertRaises(ValueError):
            check("\n".join(fixture().splitlines()[1:]))

    def test_duplicate_index(self):
        with self.assertRaises(ValueError):
            check(fixture().replace("index=64 ", "index=63 "))

    def test_time_regression(self):
        with self.assertRaises(ValueError):
            check(fixture().replace("t=1.000064000000", "t=0.000000000000"))

    def test_same_timestamp_allowed(self):
        text = fixture()
        for i in range(1, 65):
            text = text.replace(f"t={1 + i / 1000000:.12f}", "t=1.000000000000")
        self.assertEqual(check(text)["span_seconds"], 0)
