import unittest
from tools.run_simi_restore import AUTH_PASS, BINARY_PASS, HEADER_PASS, PARAMETER_PASS, PASS, RECORD_PASS, UPDATE_PASS, check

COMPLETE = "\n".join((PASS, HEADER_PASS, PARAMETER_PASS, RECORD_PASS, UPDATE_PASS, BINARY_PASS, AUTH_PASS)) + "\n"


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
                             (COMPLETE + HEADER_PASS, 0),
                             (PASS + "\n" + HEADER_PASS, 0),
                             (COMPLETE + PARAMETER_PASS, 0),
                             (COMPLETE.replace(RECORD_PASS, ""), 0),
                             (COMPLETE + RECORD_PASS, 0),
                             (COMPLETE.replace(UPDATE_PASS, ""), 0),
                             (COMPLETE + UPDATE_PASS, 0),
                             (COMPLETE.replace(BINARY_PASS, ""), 0),
                             (COMPLETE + BINARY_PASS, 0),
                             (COMPLETE.replace(AUTH_PASS, ""), 0),
                             (COMPLETE + AUTH_PASS, 0)):
            with self.subTest(text=text, status=status), self.assertRaises(ValueError):
                check(text, status)


if __name__ == "__main__":
    unittest.main()
