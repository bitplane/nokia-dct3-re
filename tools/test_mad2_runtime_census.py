import tempfile
import unittest
from pathlib import Path

from tools.mad2_runtime_census import analyze, build_payload


class Mad2RuntimeCensusTest(unittest.TestCase):
    def test_groups_first_access_records_by_direction_and_offset(self):
        text = (
            "mad2_ledger: R off=0d data=0c pc=00200010 t=0.1 clocks\n"
            "mad2_ledger: W off=0d data=2c old=0c pc=00200020 t=0.2 clocks\n"
        )
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "error.log"
            path.write_text(text)
            report = analyze("fixture", path)
        self.assertEqual(2, report["records"])
        self.assertEqual([0x0c], report["registers"][0]["values"])
        self.assertEqual("W", report["registers"][1]["direction"])

    def test_payload_quantifies_unobserved_offsets(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "error.log"
            path.write_text("mad2_ledger: R off=01 data=01 pc=00200010 t=0.1 reset\n")
            payload = build_payload([("fixture", path)])
        self.assertEqual([1], payload["coverage"]["observed_offsets"])
        self.assertIn(0, payload["coverage"]["unobserved_offsets"])


if __name__ == "__main__":
    unittest.main()
