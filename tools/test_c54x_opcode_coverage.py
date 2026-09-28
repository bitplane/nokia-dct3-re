import unittest

from tools.c54x_opcode_coverage import summarize


class C54xOpcodeCoverageTest(unittest.TestCase):
    def test_deduplicates_and_groups(self):
        result = summarize("\n".join((
            "[opcov] op=f074 first_pc=4b73",
            "[opcov] op=f072 first_pc=7ff5",
            "[opcov] op=7712 first_pc=7f2d",
            "[opcov] op=f074 first_pc=3900",
            "[opcov] op=f074 first_pc=4b73 count=9",
        )))
        self.assertEqual(result["opcodes"], 3)
        self.assertEqual(result["high_byte_groups"], 2)
        self.assertEqual(result["first_pc"][0xF074], 0x4B73)
        self.assertEqual(result["counts"][0xF074], 11)
        self.assertEqual(result["asserted"], set())

    def test_assertions_require_execution_and_are_deduplicated(self):
        result = summarize("\n".join((
            "[opcov] op=7212 first_pc=0f1d count=3",
            "[:] [opassert] op=7212",
            "[opassert] op=7212",
        )))
        self.assertEqual(result["asserted"], {0x7212})
        with self.assertRaisesRegex(ValueError, "absent from the execution trace"):
            summarize("[opcov] op=7212 first_pc=0f1d\n[opassert] op=7312")

    def test_rejects_empty_log(self):
        with self.assertRaisesRegex(ValueError, "no.*records"):
            summarize("ordinary log line")


if __name__ == "__main__":
    unittest.main()
