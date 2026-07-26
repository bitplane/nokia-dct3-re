import unittest

from tools.cobba_control_trace_check import check


class CobbaControlTraceCheckTests(unittest.TestCase):
    def test_complete_contract(self):
        self.assertEqual(
            check("cobba_fixture: control_conformance=0f\n"), 0x0F
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


if __name__ == "__main__":
    unittest.main()
