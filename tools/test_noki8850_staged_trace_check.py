import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from noki8850_staged_trace_check import check_trace


TRACE = """
staged_dsp: release entry=0f00 words=223 prom_input=0006 clock=13000000 stage=verifier
staged_dsp: publication word0=0000 word1=0006 word2=0006 word3=0006
staged_dsp: release entry=0f00 words=126 prom_input=0006 clock=13000000 stage=loader
staged_dsp: loader2_verified words=613 entry=0a00
staged_dsp: control_write port=001c data=0200
staged_dsp: outside_uploaded_code pc=2c75
staged_dsp: observation_halt pc=2c75 ownership_retained=1
8850_ready: state=01 shared_e4=0000
"""


class StagedTraceTests(unittest.TestCase):
    def test_native_boundary(self):
        self.assertEqual(check_trace(TRACE), [])

    def test_missing_publication(self):
        self.assertTrue(check_trace(TRACE.replace("publication", "unpublished")))

    def test_wrong_loader(self):
        self.assertTrue(check_trace(TRACE.replace("words=613", "words=612")))

    def test_reordered_readiness(self):
        lines = TRACE.strip().splitlines()
        self.assertTrue(check_trace("\n".join([lines[-1], *lines[:-1]])))

    def test_fatal_error(self):
        self.assertTrue(check_trace(TRACE + "Fatal error: unexpected port"))

    def test_hle_not_native(self):
        self.assertTrue(check_trace(TRACE + "runtime_hle_handoff"))


if __name__ == "__main__":
    unittest.main()
