import unittest

from tools.noki6250_staged_check import check, check_silent_runtime, check_service_control, check_lcd_stream, check_physical_ready


class StagedBoundaryTest(unittest.TestCase):
    def ready_fixture(self):
        return ("runtime_hle_handoff pc=2c75 native_suspended=1\n"
                "6250_service_control_consumer: class=74 command=0d status=00 armed=c4\n" +
                "6250_startup_fault_endpoint: bytes=" + "00" * 24 + "\n" +
                "6250_startup_fault_endpoint: bytes=" + "00" * 24 + "\n" +
                "6250_runtime_boundary: t=20.000000\n"
                "6250_scalar_post: task=1 message=0014 caller=004e967f\n"
                "6250_startup_gate: phase=03 readiness=0f t=8.000000\n"
                "6250_startup_gate: phase=03 readiness=0f t=20.000000\n"
                "6250_raw_matrix_key: value=06 pc=00505cf8 t=6.000190\n")

    def test_physical_ready(self):
        check_physical_ready(self.ready_fixture())

    def test_ready_without_input_rejected(self):
        with self.assertRaises(ValueError):
            check_physical_ready(self.ready_fixture().replace("value=06", "value=ff"))

    def test_incomplete_readiness_rejected(self):
        with self.assertRaises(ValueError):
            check_physical_ready(self.ready_fixture().replace("readiness=0f", "readiness=0e"))

    def fixture(self):
        return ("release entry=0f00 words=223 prom_input=0006 clock=13000000 stage=verifier\n"
                "publication word0=0000 word1=0006 word2=0006 word3=0006 pc=0f65\n"
                "release entry=0f00 words=126 prom_input=0006 clock=13000000 stage=loader\n"
                "request selector=0014\n" + "request selector=0001\n" * 124 +
                "loader2_verified words=613 entry=0a00\n"
                "outside_uploaded_code pc=2c75\n"
                "observation_halt pc=2c75 ownership_retained=1\n")

    def test_reviewed_boundary(self):
        check(self.fixture())

    def test_missing_verification(self):
        with self.assertRaises(ValueError):
            check(self.fixture().replace("loader2_verified", "not_verified"))

    def test_other_product_sequence(self):
        with self.assertRaises(ValueError):
            check(self.fixture() + "request selector=0001\n")

    def test_no_forced_result(self):
        with self.assertRaises(ValueError):
            check(self.fixture().replace("word0=0000", "word0=0001"))

    def runtime(self):
        commands = ["0000"] * 3 + ["8102", "900f", "8426", "920c", "920c", "920f", "920f"]
        return self.fixture() + "".join(
            f"6250_runtime_doorbell: pc=00429e48 command={command} argument=3fff pending=0001\n"
            for command in commands) + "6250_runtime_boundary: arm_pc=004c12ec dsp_pc=2c75 pending=0000\n"

    def test_silent_runtime(self):
        check_silent_runtime(self.runtime())

    def test_silent_runtime_rejects_peer_response(self):
        with self.assertRaises(ValueError):
            check_silent_runtime(self.runtime() + "RX enqueue\n")

    def service(self):
        return ("runtime_hle_handoff pc=2c75 native_suspended=1\n"
                "6250_service_control_consumer: class=74 command=0d status=00 armed=84\n"
                "6250_service_control_endpoint: flags=00 fault0=00 fault1=00\n")

    def test_service_control(self):
        check_service_control(self.service())

    def test_unarmed_completion_rejected(self):
        with self.assertRaises(ValueError):
            check_service_control(self.service().replace("armed=84", "armed=00"))

    def test_fault_not_cleared(self):
        with self.assertRaises(ValueError):
            check_service_control(self.service().replace("fault0=00", "fault0=10"))

    def lcd(self):
        commands = ["24:0", "40:0", "80:0"]
        for bank in range(1, 8):
            commands += [f"{0x40 + bank:02x}:96", "80:0"]
        commands += ["20:96"]
        return "6250_lcd_runs: data_total=768 commands=" + ",".join(commands)

    def test_eight_96_byte_banks(self):
        check_lcd_stream(self.lcd())

    def test_84_column_assumption_rejected(self):
        with self.assertRaises(ValueError):
            check_lcd_stream(self.lcd().replace(":96", ":84"))

    def test_partial_bank_rejected(self):
        with self.assertRaises(ValueError):
            check_lcd_stream(self.lcd().replace("total=768", "total=767"))


if __name__ == "__main__":
    unittest.main()
