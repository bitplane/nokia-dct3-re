import unittest

from tools.c54x_opcode_coverage import group_gaps, ranked_gaps, summarize


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

    def test_group_gaps_uses_rom4_execution_counts_and_assertion_class(self):
        rom4 = summarize("\n".join((
            "[opcov] op=4a07 first_pc=1000 count=10",
            "[opcov] op=4a12 first_pc=1001 count=4",
            "[opcov] op=4a13 first_pc=1002 count=5",
            "[opcov] op=6f82 first_pc=1003 count=30",
            "[opcov] op=7712 first_pc=1004 count=11",
        )))
        fixture = summarize("\n".join((
            "[opcov] op=4a07 first_pc=2000",
            "[opassert] op=4a07",
            "[opcov] op=4a12 first_pc=2001",
            "[opcov] op=6f82 first_pc=2002",
            "[opassert] op=6f82",
            "[opcov] op=7712 first_pc=2003",
        )))
        self.assertEqual(group_gaps(rom4, fixture), [
            (0x77, 1, 11, 0, 0),
            (0x4a, 1, 4, 1, 5),
        ])

    def test_ranked_gaps_lists_all_unasserted_words_by_observed_use(self):
        rom4 = summarize("\n".join((
            "[opcov] op=1002 first_pc=2002 count=7",
            "[opcov] op=1001 first_pc=2001 count=7",
            "[opcov] op=1003 first_pc=2003 count=3",
        )))
        fixture = summarize("\n".join((
            "[opcov] op=1001 first_pc=3001",
            "[opcov] op=1003 first_pc=3003",
            "[opassert] op=1003",
            "[opcov] op=2000 first_pc=3004",
        )))
        self.assertEqual(ranked_gaps(rom4, fixture), [
            (0x1001, 0x2001, 7, "executed-only"),
            (0x1002, 0x2002, 7, "absent"),
        ])


if __name__ == "__main__":
    unittest.main()
