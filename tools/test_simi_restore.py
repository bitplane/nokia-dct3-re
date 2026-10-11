import unittest
from tools.run_simi_restore import HEADER_PASS, PASS, check

COMPLETE = PASS + "\n" + HEADER_PASS + "\n"


class SimiRestoreResultTest(unittest.TestCase):
    def test_complete_result(self):
        check(COMPLETE, 0)

    def test_missing_duplicate_failure_and_stale_storage(self):
        for text, status in (("", 0), (COMPLETE, 3), (COMPLETE + PASS, 0),
                             (COMPLETE + "Fatal error: later failure", 0),
                             ("Error reading NVRAM file\n" + COMPLETE, 0),
                             (COMPLETE.replace("checkpoints=3", "checkpoints=2"), 0),
                             (COMPLETE.replace("security_attempts=1", "security_attempts=0"), 0),
                             (PASS, 0), (HEADER_PASS, 0),
                             (COMPLETE + HEADER_PASS, 0)):
            with self.subTest(text=text, status=status), self.assertRaises(ValueError):
                check(text, status)


if __name__ == "__main__":
    unittest.main()
