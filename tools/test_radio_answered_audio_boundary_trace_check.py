import unittest

from tools.radio_answered_audio_boundary_trace_check import verify


def control(command, value, commit, time, task="09"):
    return (
        f"dsp_shared_control: command={command} value={value} commit={commit} "
        f"caller=0028e0d0 task={task} t={time:.6f}\n")


PREFIX = (
    "dsp_hle: GSM service uplink sapi=0 pd=03 message=07 length=2 t=27.220483\n"
    + control("08", "060b", "1", 27.221398, "05")
    + "dsp_shared_write: off=0a8 old=900f data=860b "
      "pc=00291028 t=27.221423\n"
    + "dspif_transport: RX enqueue type=80 payload=34 producer=0af "
      "data=b0120000170a00010000036009030f2b2b t=27.224483\n"
)
START = (
    control("09", "08af", "1", 27.243102)
    + control("09", "09a0", "1", 27.243384)
    + control("26", "ffff", "0", 27.243445)
    + control("21", "0e10", "0", 27.243492)
    + "dsp_shared_write: off=0ae old=0000 data=0e10 "
      "pc=00291028 t=27.243517\n"
    + control("25", "0041", "0", 27.243552)
    + "dsp_shared_write: off=0b6 old=0000 data=0041 "
      "pc=00291028 t=27.243576\n"
    + control("29", "fff7", "0", 27.243600)
    + control("2f", "0000", "1", 27.243654)
)
STOP = (
    control("1c", "ffff", "0", 27.364187)
    + control("26", "0000", "0", 27.364235)
    + control("21", "0000", "0", 27.364283)
    + "dsp_shared_write: off=0ae old=0e10 data=0000 "
      "pc=00291028 t=27.364308\n"
    + control("29", "fff6", "0", 27.364346)
    + control("2f", "0000", "1", 27.364400)
)
GOOD = PREFIX + START + STOP


class AnsweredAudioBoundaryTraceCheckTest(unittest.TestCase):
    def test_answer_control_and_bounded_tone_are_classified(self):
        result = verify(GOOD)
        self.assertEqual(13, result["shared_control_commands"])
        self.assertEqual(900, result["tone_frequency_hz"])
        self.assertAlmostEqual(120.791, result["tone_duration_ms"], places=3)
        self.assertEqual(0, result["trailing_shared_control_commands"])

    def test_requires_answer_side_command(self):
        with self.assertRaisesRegex(ValueError, "answer-side lower control"):
            verify(PREFIX.replace("command=08", "command=07") + START + STOP)

    def test_rejects_unbounded_tone(self):
        with self.assertRaisesRegex(ValueError, "duration outside"):
            verify(GOOD.replace("t=27.364283", "t=27.500000"))

    def test_rejects_continuing_control_traffic(self):
        with self.assertRaisesRegex(ValueError, "continuing shared-control"):
            verify(GOOD + control("30", "fff5", "0", 28.0))


if __name__ == "__main__":
    unittest.main()
