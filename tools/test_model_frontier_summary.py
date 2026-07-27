import tempfile
import unittest
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path
from unittest.mock import patch

from tools.check_model_frontier_summary import main, read_summary


class ModelFrontierSummaryTest(unittest.TestCase):
    def test_parser(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "summary"
            path.write_text("soft_resets=0\nfiq_seen=40\n")
            self.assertEqual(
                {"soft_resets": "0", "fiq_seen": "40"}, read_summary(path)
            )

    def test_rejects_fiq0_when_negative_hardware_observes_completion(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "summary"
            path.write_text(
                "soft_resets=0\nfiq_seen=40\nlcd_full_dumps=1\n"
            )
            with patch(
                "sys.argv",
                ["check_model_frontier_summary.py", str(path), "--reject-fiq0"],
            ), redirect_stdout(StringIO()):
                self.assertEqual(1, main())

    def test_accepts_absent_fiq0_for_negative_hardware(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "summary"
            path.write_text(
                "soft_resets=0\nfiq_seen=21\nlcd_full_dumps=1\n"
            )
            with patch(
                "sys.argv",
                ["check_model_frontier_summary.py", str(path), "--reject-fiq0"],
            ), redirect_stdout(StringIO()):
                self.assertEqual(0, main())
