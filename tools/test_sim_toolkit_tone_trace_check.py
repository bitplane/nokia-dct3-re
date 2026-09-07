import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sim_toolkit_tone_trace_check as check


GOOD = "\n".join([
    "proactive PLAY TONE ready",
    "SIM status ins=c2 sw=911c",
    "dsp_tone: frequency=1750/0 amplitude=65ac active=1/0",
    "dsp_tone: frequency=0/0 amplitude=65ac active=0/0",
    "terminal-response data=81030c200002028281030111",
    "SIM status ins=14 sw=9000",
])


class SimToolkitToneTraceCheckTest(unittest.TestCase):
    def test_playback_and_cancel_pass(self):
        check.verify(GOOD)

    def test_missing_tone_stop_fails(self):
        with self.assertRaisesRegex(ValueError, "frequency=0"):
            check.verify(GOOD.replace("frequency=0/0", "frequency=1/0"))


if __name__ == "__main__":
    unittest.main()
