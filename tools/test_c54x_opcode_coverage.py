import unittest

from tools.c54x_opcode_coverage import summarize


class C54xOpcodeCoverageTest(unittest.TestCase):
    def test_deduplicates_and_groups(self):
        result = summarize("\n".join((
            "[opcov] op=f074 first_pc=4b73",
            "[opcov] op=f072 first_pc=7ff5",
            "[opcov] op=7712 first_pc=7f2d",
            "[opcov] op=f074 first_pc=3900",
        )))
        self.assertEqual(result["opcodes"], 3)
        self.assertEqual(result["high_byte_groups"], 2)
        self.assertEqual(result["first_pc"][0xF074], 0x4B73)

    def test_rejects_empty_log(self):
        with self.assertRaisesRegex(ValueError, "no.*records"):
            summarize("ordinary log line")


if __name__ == "__main__":
    unittest.main()
