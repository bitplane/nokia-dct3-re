import unittest
from tools.run_simi_restore import ADM_PASS, AOC_PASS, DCS_PASS
from tools.run_simi_restore import ACCESS_PASS, AUTH_ACCESS_PASS, AUTH_PASS, BINARY_ACCESS_PASS, BINARY_PASS, BLOCKED_DISABLED_PASS, HEADER_PASS, IMSI_PASS, PARAMETER_PASS, PASS, PLMN_PASS, PUCT_PASS, SMSS_PASS, RECORD_PASS, RESPONSE_PASS, SEARCH_PASS, UPDATE_PASS, check

COMPLETE = "\n".join((PASS, HEADER_PASS, PARAMETER_PASS, RECORD_PASS, UPDATE_PASS, BINARY_PASS, AUTH_PASS, RESPONSE_PASS, SEARCH_PASS, ACCESS_PASS, IMSI_PASS, BINARY_ACCESS_PASS, AUTH_ACCESS_PASS, BLOCKED_DISABLED_PASS, PLMN_PASS, PUCT_PASS, SMSS_PASS, ADM_PASS, AOC_PASS, DCS_PASS)) + "\n"


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
                             (COMPLETE + AUTH_PASS, 0),
                             (COMPLETE.replace(RESPONSE_PASS, ""), 0),
                             (COMPLETE + RESPONSE_PASS, 0),
                             (COMPLETE.replace(SEARCH_PASS, ""), 0),
                             (COMPLETE + SEARCH_PASS, 0),
                             (COMPLETE.replace(ACCESS_PASS, ""), 0),
                             (COMPLETE + ACCESS_PASS, 0),
                             (COMPLETE.replace(IMSI_PASS, ""), 0),
                             (COMPLETE.replace(BINARY_ACCESS_PASS, ""), 0),
                             (COMPLETE.replace('files=9 before_pin=1', 'files=8 before_pin=1'), 0),
                             (COMPLETE.replace(AUTH_ACCESS_PASS, ""), 0),
                             (COMPLETE.replace(BLOCKED_DISABLED_PASS, ""), 0),
                             (COMPLETE.replace(PLMN_PASS, ""), 0),
                             (COMPLETE + PLMN_PASS, 0),
                             (COMPLETE.replace(PUCT_PASS, ""), 0),
                             (COMPLETE + PUCT_PASS, 0),
                             (COMPLETE.replace(SMSS_PASS, ""), 0),
                             (COMPLETE + SMSS_PASS, 0),
                             (COMPLETE.replace(ADM_PASS, ""), 0),
                             (COMPLETE.replace('updates: PASS files=4', 'updates: PASS files=3'), 0),
                             (COMPLETE + ADM_PASS, 0),
                             (COMPLETE.replace(AOC_PASS, ""), 0),
                             (COMPLETE.replace(DCS_PASS, ""), 0),
                             (COMPLETE + DCS_PASS, 0),
                             (COMPLETE + AOC_PASS, 0),
                             (COMPLETE + BLOCKED_DISABLED_PASS, 0),
                             (COMPLETE + AUTH_ACCESS_PASS, 0),
                             (COMPLETE + BINARY_ACCESS_PASS, 0),
                             (COMPLETE.replace("reset_revokes=1", "reset_revokes=0"), 0),
                             (COMPLETE.replace("removal_revokes=1", "removal_revokes=0"), 0),
                             (COMPLETE + IMSI_PASS, 0)):
            with self.subTest(text=text, status=status), self.assertRaises(ValueError):
                check(text, status)


if __name__ == "__main__":
    unittest.main()
