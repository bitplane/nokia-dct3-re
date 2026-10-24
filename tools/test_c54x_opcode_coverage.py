import unittest
from contextlib import redirect_stderr, redirect_stdout
from io import StringIO
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch

from tools.c54x_opcode_coverage import (decoder_declared_words, group_gaps, main,
                                        ranked_gaps, ranked_variant_candidates, summarize,
                                        require_rom4_idle, require_extension_capture,
                                        require_extension_decoding, decoded_boundaries)


class C54xOpcodeCoverageTest(unittest.TestCase):
    def test_malformed_primary_records_cannot_disappear_from_coverage(self):
        valid = '[opcov] op=8194 first_pc=05e3 count=1\n'
        for record in (
            '[opcov] op=8194 first_pc=05e3 count=-1',
            '[opcov] op=8194 first_pc=05e3 count=1 trailing',
            '[opcov] op=10000 first_pc=05e3 count=1',
            '[opassert] op=8194 trailing',
            '[:] [opassert] op=819',
            '[opdecode] op=8194 supported=2 words=1',
            '[opdecode] op=8194 supported=1 words=1 trailing',
        ):
            with self.subTest(record=record), self.assertRaisesRegex(ValueError, 'malformed'):
                summarize(valid + record + '\n')

    def test_unrelated_log_lines_and_both_assertion_formats_remain_supported(self):
        result = summarize('ordinary MAME output\n'
                           '[opcov] op=8194 first_pc=05e3 count=1\n'
                           '[opassert] op=8194\n'
                           '[:] [opassert] op=8194\n'
                           '[opdecode] op=8194 supported=1 words=1\n')
        self.assertEqual(result['asserted'], {0x8194})
        self.assertEqual(result['decoded'], {0x8194: (True, 1)})

    def test_completed_fixture_union_requires_assertions_not_just_execution(self):
        with TemporaryDirectory() as root:
            root = Path(root)
            observed, first, second = (root / name for name in ('observed', 'first', 'second'))
            observed.write_text('[opcov] op=8194 first_pc=a66c count=10\n[opcov] op=f48c first_pc=a6c3 count=10\n')
            first.write_text('[opcov] op=8194 first_pc=05e3 count=1\n[opassert] op=8194\n')
            second.write_text('[opcov] op=f48c first_pc=0600 count=1\n[opassert] op=f48c\n')
            args = ['coverage', str(observed), '--fixture-log', str(first), '--require-all-asserted']
            with patch('sys.argv', args), redirect_stdout(StringIO()), redirect_stderr(StringIO()):
                self.assertEqual(main(), 1)
            with patch('sys.argv', [*args, '--additional-fixture-log', str(second)]), redirect_stdout(StringIO()), redirect_stderr(StringIO()):
                self.assertEqual(main(), 0)
            second.write_text('[opcov] op=f48c first_pc=0600 count=1\n')
            with patch('sys.argv', [*args, '--additional-fixture-log', str(second)]), redirect_stdout(StringIO()), redirect_stderr(StringIO()):
                self.assertEqual(main(), 1)
            first.write_text(first.read_text() + '[opassert] op=f48c\n')
            with patch('sys.argv', [*args, '--additional-fixture-log', str(second)]), redirect_stdout(StringIO()), redirect_stderr(StringIO()):
                self.assertEqual(main(), 1)

    def test_additional_fixture_requires_primary_fixture(self):
        with patch('sys.argv', ['coverage', 'observed', '--additional-fixture-log', 'extra']), redirect_stderr(StringIO()):
            with self.assertRaises(SystemExit) as error:
                main()
            self.assertEqual(error.exception.code, 2)

    def test_tuple_decoding_requires_every_captured_variant(self):
        capture = ("[opcov] op=6f83 first_pc=0800 count=2\n"
                   "[opwords] op=6f83 words=2 ext1=0c1e ext2=0000 first_pc=010800 count=1\n"
                   "[opwords] op=6f83 words=2 ext1=0c5f ext2=0000 first_pc=010810 count=1\n")
        first = "[optupledecode] op=6f83 words=2 ext1=0c1e ext2=0000 supported=1 decoded_words=2\n"
        second = "[optupledecode] op=6f83 words=2 ext1=0c5f ext2=0000 supported=1 decoded_words=2\n"
        with self.assertRaisesRegex(ValueError, "every fetched variant"):
            require_extension_decoding(summarize(capture + first))
        self.assertEqual(decoded_boundaries(summarize(capture + first)), set())
        require_extension_decoding(summarize(capture + first + second))
        self.assertEqual(decoded_boundaries(summarize(capture + first + second)), {0x6f83})
        with self.assertRaisesRegex(ValueError, "undecoded"):
            require_extension_decoding(summarize(capture + first + second.replace("supported=1 decoded_words=2", "supported=0 decoded_words=0")))

    def test_tuple_decoder_rejects_invalid_lengths_conflicts_or_orphans(self):
        capture = ("[opcov] op=6f83 first_pc=0800 count=1\n"
                   "[opwords] op=6f83 words=2 ext1=0c1e ext2=0000 first_pc=010800 count=1\n")
        decoded = "[optupledecode] op=6f83 words=2 ext1=0c1e ext2=0000 supported=1 decoded_words=2\n"
        records = (
            decoded.replace("decoded_words=2", "decoded_words=3"),
            decoded.replace("supported=1", "supported=0"),
            decoded.replace("ext1=0c1e", "ext1=0c5f"),
            "[optupledecode] op=6f83 words=2\n",
            decoded + decoded.replace("supported=1 decoded_words=2", "supported=0 decoded_words=0"),
        )
        for record in records:
            with self.subTest(record=record), self.assertRaises(ValueError):
                summarize(capture + record)

    def test_cli_fails_when_real_tuple_is_undecoded_despite_first_word_support(self):
        with TemporaryDirectory() as root:
            log = Path(root) / "trace.log"
            log.write_text("[opcov] op=6f83 first_pc=0800 count=1\n"
                           "[opdecode] op=6f83 supported=1 words=2\n"
                           "[opwords] op=6f83 words=2 ext1=0c1e ext2=0000 first_pc=010800 count=1\n"
                           "[optupledecode] op=6f83 words=2 ext1=0c1e ext2=0000 supported=0 decoded_words=0\n")
            with patch("sys.argv", ["coverage", str(log), "--require-extension-decoded"]), redirect_stderr(StringIO()):
                self.assertEqual(main(), 1)
            self.assertEqual(decoded_boundaries(summarize(log.read_text())), set())

    def test_extension_tuples_preserve_variants_and_count_repeated_runs(self):
        text = ("[opcov] op=6f82 first_pc=0800 count=3\n"
                "[opwords] op=6f82 words=2 ext1=0c48 ext2=0000 first_pc=010800 count=2\n"
                "[opwords] op=6f82 words=2 ext1=0e20 ext2=0000 first_pc=010810 count=1\n")
        result = summarize(text + text)
        require_extension_capture(result)
        self.assertEqual(result["extensions"], {
            (0x6f82, 2, 0x0c48, 0): (0x010800, 4),
            (0x6f82, 2, 0x0e20, 0): (0x010810, 2),
        })

    def test_extension_records_reject_malformed_or_impossible_data(self):
        records = (
            "[opwords] op=6f82 words=2 ext1=0c48 ext2=0001 first_pc=010800 count=1",
            "[opwords] op=6fe2 words=2 ext1=0c48 ext2=0000 first_pc=010800 count=1",
            "[opwords] op=6f82 words=3 ext1=0c48 ext2=0000 first_pc=010800 count=1",
            "[opwords] op=6f82 words=2 ext1=0c48 ext2=0000 first_pc=800000 count=1",
            "[opwords] op=6f82 words=2 ext1=0c48 ext2=0000 first_pc=010800 count=0",
            "[opwords] op=7c82 words=2 ext1=0c48 ext2=0000 first_pc=010800 count=1",
            "[opwords] op=6f82 words=2 ext1=0c48",
        )
        for record in records:
            with self.subTest(record=record), self.assertRaises(ValueError):
                summarize("[opcov] op=6f82 first_pc=0800 count=1\n" + record)

    def test_extension_capture_rejects_orphans_missing_variants_and_count_drift(self):
        record = "[opwords] op=6f82 words=2 ext1=0c48 ext2=0000 first_pc=010800 count=1\n"
        with self.assertRaisesRegex(ValueError, "absent"):
            summarize("[opcov] op=7712 first_pc=0800\n" + record)
        for text in ("[opcov] op=6f82 first_pc=0800 count=1\n",
                     "[opcov] op=6f82 first_pc=0800 count=2\n" + record,
                     "[opcov] op=6f82 first_pc=0800 count=1\n" + record +
                     "[opcov] op=6f83 first_pc=0802 count=1\n"):
            with self.subTest(text=text), self.assertRaises(ValueError):
                require_extension_capture(summarize(text))

    def test_extension_report_uses_real_words_not_synthetic_decoder_support(self):
        with TemporaryDirectory() as root:
            log = Path(root) / "trace.log"
            log.write_text("[opcov] op=6ff8 first_pc=ffff count=1\n"
                           "[opdecode] op=6ff8 supported=0 words=0\n"
                           "[opwords] op=6ff8 words=3 ext1=1234 ext2=0c48 first_pc=01ffff count=1\n")
            output = StringIO()
            with patch("sys.argv", ["coverage", str(log), "--extension-report"]), redirect_stdout(output):
                self.assertEqual(main(), 0)
            self.assertIn("Fetched 6f instruction tuples: 1", output.getvalue())
            self.assertIn("ext1=1234 ext2=0c48 first_pc=01ffff executions=1", output.getvalue())

    def test_current_fingerprint_rejects_previous_native_trace(self):
        current = {"opcodes": 626, "high_byte_groups": 110,
                   "set_sha256": "05356c59d7b0843b8023b7a68822b1b258a2a18cd161db6dd33955580bf33a59"}
        require_rom4_idle(current)
        with self.assertRaisesRegex(ValueError, "coverage mismatch"):
            require_rom4_idle({"opcodes": 594, "high_byte_groups": 107,
                               "set_sha256": "5ec81f25976d365d2bbfe09037d70ac44676b5803535892d8237744c6a167d4e"})
        with self.assertRaisesRegex(ValueError, "coverage mismatch"):
            require_rom4_idle(dict(current, set_sha256="0" * 64))

    def test_frontend_report_requires_complete_actual_decode_inventory(self):
        with TemporaryDirectory() as root:
            log = Path(root) / "trace.log"
            log.write_text("[opcov] op=7712 first_pc=0800 count=3\n")
            with patch("sys.argv", ["coverage", str(log), "--frontend-report"]), redirect_stderr(StringIO()):
                self.assertEqual(main(), 1)
            log.write_text(log.read_text() + "[opdecode] op=7712 supported=0 words=0\n")
            output = StringIO()
            with patch("sys.argv", ["coverage", str(log), "--frontend-report"]), redirect_stdout(output):
                self.assertEqual(main(), 0)
            self.assertIn("decoded=0 unsupported=1 observed=1", output.getvalue())
            self.assertIn("executions=3 length=unknown", output.getvalue())

    def test_actual_decoder_records_do_not_promote_raw_word_lengths(self):
        result = summarize("[opcov] op=7712 first_pc=0800 count=3\n"
                           "[opdecode] op=7712 supported=1 words=2\n"
                           "[opcov] op=1234 first_pc=0802 count=5\n"
                           "[opdecode] op=1234 supported=0 words=0\n")
        self.assertEqual(result["decoded"], {0x7712: (True, 2), 0x1234: (False, 0)})

    def test_rejects_invalid_or_conflicting_decoder_records(self):
        for records in ("[opdecode] op=7712 supported=0 words=1",
                        "[opdecode] op=7712 supported=1 words=0",
                        "[opdecode] op=7712 supported=1 words=4",
                        "[opdecode] op=7712 supported=1 words=2\n"
                        "[opdecode] op=7712 supported=0 words=0"):
            with self.assertRaises(ValueError):
                summarize("[opcov] op=7712 first_pc=0800\n" + records)
        with self.assertRaisesRegex(ValueError, "absent"):
            summarize("[opcov] op=7712 first_pc=0800\n[opdecode] op=1234 supported=0 words=0")

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
        with self.assertRaisesRegex(ValueError, "positive"):
            summarize("[opcov] op=7212 first_pc=0f1d count=0\n[opassert] op=7212")

    def test_rejects_empty_log(self):
        with self.assertRaisesRegex(ValueError, "no.*records"):
            summarize("ordinary log line")

    def test_decoder_inventory_distinguishes_group_and_exact_cases(self):
        source = """void tms320c54x_device::execute_one(u16 op)
if ((op & 0xfff0) == 0xabc0) return;
if (op == 0x4567) return;
switch (op & 0xff00) {
case 0x1200: return;
}
switch (op) {
case 0xfc00: return;
}
void tms320c54x_device::execute_run() {}
"""
        declared = decoder_declared_words(source)
        self.assertEqual(len(declared), 16 + 256 + 2)
        self.assertTrue({0xabc7, 0x12fe, 0xfc00, 0x4567} <= declared)
        self.assertFalse({0xfc01, 0xabd0, 0x1300} & declared)

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

    def test_variant_candidates_rank_observed_families_without_claiming_decode(self):
        rom4 = summarize("\n".join((
            "[opcov] op=1001 first_pc=2001 count=4",
            "[opcov] op=1002 first_pc=2002 count=6",
            "[opcov] op=2001 first_pc=2003 count=8",
        )))
        fixture = summarize("[opcov] op=1001 first_pc=3001\n"
                            "[opcov] op=2001 first_pc=3002\n")
        declared = {0x1001, 0x1002, 0x1003, 0x2001, 0x2002, 0x3001}
        self.assertEqual(ranked_variant_candidates(rom4, fixture, declared), [
            (0x10, 4, 10, 0x1002, 2, 2),
            (0x20, 0, 8, 0x2001, 1, 1),
        ])

    def test_variant_ranking_discounts_a_hot_nop(self):
        rom4 = summarize("\n".join((
            "[opcov] op=f495 first_pc=1000 count=1000",
            "[opcov] op=f400 first_pc=1001 count=1",
            "[opcov] op=1001 first_pc=1002 count=60",
            "[opcov] op=1002 first_pc=1003 count=40",
        )))
        fixture = summarize("[opcov] op=f495 first_pc=2000\n"
                            "[opcov] op=1001 first_pc=2001\n")
        declared = {0xf495, 0xf400, 0xf401, 0x1001, 0x1002, 0x1003}
        self.assertEqual([row[0] for row in ranked_variant_candidates(
            rom4, fixture, declared)], [0x10, 0xf4])

    def test_require_all_asserted_rejects_missing_and_executed_only_words(self):
        with TemporaryDirectory() as directory:
            rom4 = Path(directory) / "rom4.log"
            fixture = Path(directory) / "fixture.log"
            rom4.write_text("[opcov] op=1001 first_pc=2000\n"
                            "[opcov] op=1002 first_pc=2001\n")
            fixture.write_text("[opcov] op=1001 first_pc=3000\n")
            with patch("sys.argv", ["coverage", str(rom4), "--fixture-log",
                                    str(fixture), "--require-all-asserted"]):
                with redirect_stdout(StringIO()), redirect_stderr(StringIO()):
                    self.assertEqual(main(), 1)
            fixture.write_text("[opcov] op=1001 first_pc=3000\n"
                               "[opcov] op=1002 first_pc=3001\n"
                               "[opassert] op=1001\n")
            with patch("sys.argv", ["coverage", str(rom4), "--fixture-log",
                                    str(fixture), "--require-all-asserted"]):
                with redirect_stdout(StringIO()), redirect_stderr(StringIO()):
                    self.assertEqual(main(), 1)
            fixture.write_text("[opcov] op=1001 first_pc=3000\n"
                               "[opcov] op=1002 first_pc=3001\n"
                               "[opassert] op=1001\n[opassert] op=1002\n")
            with patch("sys.argv", ["coverage", str(rom4), "--fixture-log",
                                    str(fixture), "--require-all-asserted"]):
                with redirect_stdout(StringIO()), redirect_stderr(StringIO()):
                    self.assertEqual(main(), 0)

    def test_additional_trace_adds_observed_words_to_assertion_gate(self):
        with TemporaryDirectory() as directory:
            idle = Path(directory) / "idle.log"
            menu = Path(directory) / "menu.log"
            fixture = Path(directory) / "fixture.log"
            idle.write_text("[opcov] op=1001 first_pc=2000 count=3\n")
            menu.write_text("[opcov] op=1001 first_pc=2000 count=5\n"
                            "[opcov] op=1002 first_pc=2001 count=7\n")
            fixture.write_text("[opcov] op=1001 first_pc=3000\n[opassert] op=1001\n")
            argv = ["coverage", str(idle), "--additional-log", str(menu),
                    "--fixture-log", str(fixture), "--require-all-asserted"]
            with patch("sys.argv", argv):
                with redirect_stdout(StringIO()), redirect_stderr(StringIO()):
                    self.assertEqual(main(), 1)
            fixture.write_text("[opcov] op=1001 first_pc=3000\n[opassert] op=1001\n"
                               "[opcov] op=1002 first_pc=3001\n[opassert] op=1002\n")
            output = StringIO()
            with patch("sys.argv", argv), redirect_stdout(output), redirect_stderr(StringIO()):
                self.assertEqual(main(), 0)
            self.assertIn("opcodes=2", output.getvalue())
            self.assertIn("new_vs_idle=1", output.getvalue())

    def test_observed_group_reports_exact_counts_and_fixture_class(self):
        with TemporaryDirectory() as directory:
            rom4 = Path(directory) / "rom4.log"
            fixture = Path(directory) / "fixture.log"
            rom4.write_text("[opcov] op=f495 first_pc=1000 count=9\n"
                            "[opcov] op=f400 first_pc=1001 count=2\n")
            fixture.write_text("[opcov] op=f495 first_pc=2000\n"
                               "[opassert] op=f495\n")
            output = StringIO()
            with patch("sys.argv", ["coverage", str(rom4), "--fixture-log",
                                    str(fixture), "--observed-group", "f4"]), \
                    redirect_stdout(output):
                self.assertEqual(main(), 0)
            self.assertIn("op=f495 first_pc=1000 executions=9 fixture=asserted",
                          output.getvalue())
            self.assertIn("op=f400 first_pc=1001 executions=2 fixture=absent",
                          output.getvalue())


if __name__ == "__main__":
    unittest.main()
