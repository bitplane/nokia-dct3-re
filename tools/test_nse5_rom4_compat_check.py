import unittest

from tools import nse5_rom4_compat_check as checker


def complete_trace():
    return "\n".join(
        f"nse5_compat_sample: t={t:.6f} pc=0049fff8 result0=0016 result1=0004"
        for t in (0.1, 0.5, 1, 2, 4, 8)) + (
        "\nrom4_interface_summary: completion_strobes=94 mailbox_writes=235")


class CompatibilityCheckTest(unittest.TestCase):
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
