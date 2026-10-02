import unittest

from tools import nse5_rom4_compat_check as checker


def complete_trace():
    return "\n".join(
        f"nse5_compat_sample: t={t:.6f} pc=0049fff8 result0=0016 result1=0004"
        for t in (0.1, 0.5, 1, 2, 4, 8)) + (
        "\nrom4_interface_summary: completion_strobes=94 mailbox_writes=235") + "\n" + "\n".join(
        f"nse5_compat_entry: name={name} pc=00432eae r0=00000000 r1=00000000 lr=0049ff17 t=0.1\n"
        f"nse5_compat_entries: name={name} count=1" for name in ("verifier", "service_init"))


class CompatibilityCheckTest(unittest.TestCase):
    def test_upload_read_mismatch(self):
        trace = ("nse5_compat_dsp_upper_program_upload: address=282d value=4a08\n"
                 "nse5_compat_dsp_live_word: address=282d word=2833")
        result = checker.program_upload_observation(trace)
        self.assertEqual(result["mismatches"][0]["address"], 0x282d)

    def test_upload_read_matches_latest_write(self):
        trace = ("nse5_compat_dsp_upper_program_upload: address=282d value=4a08\n"
                 "nse5_compat_dsp_upper_program_upload: address=282d value=fc00\n"
                 "nse5_compat_dsp_live_word: address=282d word=fc00")
        result = checker.program_upload_observation(trace)
        self.assertEqual(result["captured_write_addresses"], 1)
        self.assertEqual(len(result["compared_reads"]), 1)
        self.assertFalse(result["mismatches"])

    def test_uncaptured_write_does_not_prove_match(self):
        result = checker.program_upload_observation(
            "nse5_compat_dsp_live_word: address=282d word=fc00")
        self.assertEqual(result["compared_reads"], [])

    def test_queue_full_at_selftest_reply(self):
        trace = ("nse5_compat_task2_queue: primitive=0d detail=00 producer=0b "
                 "consumer=00 capacity=0c t=0.701839\n"
                 "nse5_compat_task2_queue_failure: primitive=0d detail=00 t=0.701846")
        result = checker.task2_queue_observation(trace)
        self.assertTrue(result["posts"][0]["full"])
        self.assertEqual(result["failures"][0]["primitive"], 13)

    def test_queue_full_wraps_at_capacity(self):
        trace = ("nse5_compat_task2_queue: primitive=32 detail=00 producer=02 "
                 "consumer=03 capacity=0c t=1.0")
        self.assertTrue(checker.task2_queue_observation(trace)["posts"][0]["full"])

    def test_empty_queue_is_not_full(self):
        trace = ("nse5_compat_task2_queue: primitive=32 detail=00 producer=00 "
                 "consumer=00 capacity=0c t=1.0")
        self.assertFalse(checker.task2_queue_observation(trace)["posts"][0]["full"])

    def test_invalid_queue_index_rejected(self):
        trace = ("nse5_compat_task2_queue: primitive=0d detail=00 producer=0c "
                 "consumer=00 capacity=0c t=1.0")
        with self.assertRaisesRegex(ValueError, "geometry"):
            checker.task2_queue_observation(trace)

    def test_executed_observation_window(self):
        self.assertEqual(len(checker.check_trace(complete_trace(), 0)), 6)

    def test_process_failure_rejected(self):
        with self.assertRaises(ValueError):
            checker.check_trace(complete_trace(), 1)

    def test_lua_failure_rejected(self):
        with self.assertRaises(ValueError):
            checker.check_trace(complete_trace() + "[LUA ERROR]", 0)

    def test_missing_sample_rejected(self):
        with self.assertRaises(ValueError):
            checker.check_trace(complete_trace().split("\n", 1)[1], 0)

    def test_missing_dsp_completion_rejected(self):
        with self.assertRaises(ValueError):
            checker.check_trace(complete_trace().replace("completion_strobes=94", "completion_strobes=0"), 0)

    def test_wrong_executed_version_rejected(self):
        with self.assertRaises(ValueError):
            checker.check_trace(complete_trace().replace("result1=0004", "result1=ffff"), 0)

    def test_counter_without_successful_tap_detail_rejected(self):
        with self.assertRaisesRegex(ValueError, "positive-control"):
            checker.check_trace(complete_trace().replace("name=verifier pc=", "name=other pc="), 0)
