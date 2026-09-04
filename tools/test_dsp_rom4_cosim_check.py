import unittest

from tools.dsp_rom4_cosim_check import parse_log


BASE = """
[dsp54] loaded DROM /private/dsp_drom.txt (16384 words)
[dsp54] loaded /private/dsp_full.bin (131070 bytes) cosim=1
[dsp54] REALUP: DSP RELEASED
[dsp54] PORTW pa=0x2C val=0x001F
[dsp54] PORTR pa=0x2D -> 0x0000
[dsp54] HINT->IRQ4 #1: DSP doorbell
[dsp54] RINGFIQ FIQ1: MDISND head -> 0x06
[dsp54] COBBA: codec frame RINT0 (vec20)
[dsp54] reach4: superloop=1 idle=2 bist=3 daram=4 retpad=5 other=85
  DSP acks=7
=== stopped: budget reached ===
=== LCD framebuffer (1 data bytes) ===
"""


class DspRom4CosimCheckTest(unittest.TestCase):
    def test_classifies_reproduction_and_assists(self):
        result = parse_log(BASE + "[dsp54] SELFTEST_MEAS: nominal idle measurement staged\n")
        self.assertEqual(result["executed_instructions"], 100)
        self.assertEqual(result["regions"]["other"]["percent"], 85.0)
        self.assertTrue(result["modeled_assists_observed"]["selftest_measurement_patch"])
        self.assertTrue(result["promotion_blocked"])
        self.assertEqual(result["interface_activity"]["dsp_acknowledgements"], 7)
        self.assertEqual(result["interface_activity"]["ring_transitions"], 1)
        self.assertEqual(result["interface_activity"]["host_doorbells"], 1)
        self.assertEqual(result["interface_activity"]["codec_frame_interrupts"], 1)
        self.assertEqual(result["interface_activity"]["port_accesses"], {
            "w_0x2c": 1,
            "r_0x2d": 1,
        })

    def test_clean_log_has_no_observed_assist_block(self):
        self.assertFalse(parse_log(BASE)["promotion_blocked"])

    def test_requires_histogram_summary(self):
        with self.assertRaisesRegex(ValueError, "reach4"):
            parse_log(BASE.replace("[dsp54] reach4: superloop=1 idle=2 bist=3 daram=4 retpad=5 other=85\n", ""))


if __name__ == "__main__":
    unittest.main()
