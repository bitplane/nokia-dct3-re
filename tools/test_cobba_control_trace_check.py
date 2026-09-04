import unittest

from tools.cobba_control_trace_check import check


class CobbaControlTraceCheckTests(unittest.TestCase):
    def test_complete_contract(self):
        self.assertEqual(
            check("""cobba: control sequence=1 direction=write address=3 data=ace t=0.1
cobba: control sequence=2 direction=read address=3 data=ace t=0.1
cobba: control sequence=3 direction=write address=4 data=123 t=0.1
cobba: control sequence=4 direction=write address=5 data=fed t=0.1
cobba: control sequence=5 direction=read address=5 data=fed t=0.1
cobba_fixture: control_conformance=0f
"""), 0x0F
        )

    def test_partial_contract_fails(self):
        with self.assertRaisesRegex(ValueError, "expected exactly 0f"):
            check("cobba_fixture: control_conformance=07\n")

    def test_duplicate_result_fails(self):
        with self.assertRaisesRegex(ValueError, "expected exactly 0f"):
            check(
                "cobba_fixture: control_conformance=0f\n"
                "cobba_fixture: control_conformance=0f\n"
            )

    def test_missing_result_fails(self):
        with self.assertRaisesRegex(ValueError, "missing"):
            check("")

    def test_missing_transactions_fail(self):
        with self.assertRaisesRegex(ValueError, "ordered control transactions"):
            check("cobba_fixture: control_conformance=0f\n")


if __name__ == "__main__":
    unittest.main()
