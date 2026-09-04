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
cobba: control sequence=6 direction=write address=8 data=610 t=0.1
cobba: codec serial loopback data=0aaa count=1 t=0.1
cobba: control sequence=7 direction=write address=8 data=000 t=0.1
cobba_fixture: control_conformance=1f
"""), 0x1F
        )

    def test_partial_contract_fails(self):
        with self.assertRaisesRegex(ValueError, "expected exactly 1f"):
            check("cobba_fixture: control_conformance=0f\n")

    def test_duplicate_result_fails(self):
        with self.assertRaisesRegex(ValueError, "expected exactly 1f"):
            check(
                "cobba_fixture: control_conformance=1f\n"
                "cobba_fixture: control_conformance=1f\n"
            )

    def test_missing_result_fails(self):
        with self.assertRaisesRegex(ValueError, "missing"):
            check("")

    def test_missing_transactions_fail(self):
        with self.assertRaisesRegex(ValueError, "ordered control transactions"):
            check("cobba_fixture: control_conformance=1f\n")


if __name__ == "__main__":
    unittest.main()
