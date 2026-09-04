import unittest

from dsp_rom4_upload_trace_check import check_trace


BLOCKS = """
 1: DAT_00201000 25b4 1f80 034e 0130 044c 0000
 2: DAT_00201010 216a 1e80 011c 0130 0078 0000
 3: DAT_00201020 2286 1e80 04ec 0130 04ec 0000
 8: DAT_00201030 2286 1e80 0118 0130 0118 0000
18: DAT_00201040 0d80 1e80 01f4 0130 01f4 0000
"""

TRACE = """
REALUP: DSP RELEASED
REALUP: DSP HELD in reset
REALUP: WARM BOOT CPU reset + DARAM PRESERVED
HDRLOG #1 hdr7F=0x0078 @934k hdr[7B..7F]=FD00 FF80 0244 0500 0078
[reqwatch] pc=0x0F41 REQ[0x871]=0x0012
HDRLOG #2 hdr7F=0x01F4 @938k hdr[7B..7F]=0D80 1000 0122 0580 01F4
HDRLOG #3 reply=0x0004 @938k
[reqwatch] pc=0x0D80 REQ[0x871]=0x0000
[reqwatch] pc=0x3855 REQ[0x871]=0x0001
HDRLOG #4 hdr7F=0x044C @938k hdr[7B..7F]=25B4 1F80 034E 0130 044C
HDRLOG #5 reply=0x0002 @944k
HDRLOG #6 hdr7F=0x0078 @944k
HDRLOG #7 hdr7F=0x0118 @945k
HDRLOG #8 hdr7F=0x04EC @946k
"""


class UploadTraceCheckTests(unittest.TestCase):
    def test_accepts_complete_lifecycle(self):
        self.assertEqual(check_trace(TRACE, BLOCKS), [])

    def test_rejects_missing_request(self):
        errors = check_trace(TRACE.replace("0x0012", "0x0000"), BLOCKS)
        self.assertIn("missing loader request 0x0012", errors)

    def test_rejects_uncatalogued_header(self):
        errors = check_trace(TRACE.replace("0x04EC @946k", "0x0999 @946k"), BLOCKS)
        self.assertTrue(any("absent from block catalogue" in error for error in errors))

    def test_rejects_missing_descriptor(self):
        errors = check_trace(TRACE.replace("25B4 1F80", "25B5 1F80"), BLOCKS)
        self.assertTrue(any("missing upload descriptor" in error for error in errors))

    def test_rejects_retired_daram_assist(self):
        errors = check_trace(TRACE + "SEEDDARAM: reloaded resident words\n", BLOCKS)
        self.assertIn("trace still uses the retired DARAM reseed assist", errors)


if __name__ == "__main__":
    unittest.main()
