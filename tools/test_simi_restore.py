import unittest
from tools.run_simi_restore import PASS, check


class SimiRestoreResultTest(unittest.TestCase):
    def test_complete_result(self):
        check(PASS + "\n", 0)

    def test_missing_duplicate_failure_and_stale_storage(self):
        for text, status in (("", 0), (PASS, 3), (PASS + "\n" + PASS, 0),
                             (PASS + "\nFatal error: later failure", 0),
                             ("Error reading NVRAM file\n" + PASS, 0),
                             (PASS.replace("checkpoints=2", "checkpoints=1"), 0)):
            with self.subTest(text=text, status=status), self.assertRaises(ValueError):
                check(text, status)


if __name__ == "__main__":
    unittest.main()
