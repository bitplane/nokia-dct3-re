import unittest

from tools.c54x_rom4_processing_check import check
from tools.test_c54x_rom4_rf_boundary_check import summary
from tools import test_c54x_rom4_rf_boundary_check as rf_fixtures


def processing_rf_summary():
    # This synthetic processing workload has its own sample/frame counts;
    # do not inherit the shorter coherent-boot fixture's default duration.
    return summary(frames=6499, reads=207040)


def fixture():
    frames = 6033
    counts = {0x330a: frames * 8, 0x3347: frames - 2, 0x3357: frames - 2,
              0x3382: 3017, 0x3385: 3017, 0x33a1: 0, 0x33a4: 0, 0x33ac: 0}
    counts.update({address: frames for address in (0x3360, 0x3362, 0x336c, 0x336e, 0x2400, 0x2402)})
    lines = [processing_rf_summary(), f"rom4_dispatch_count: address=32f4 count={frames}"]
    lines += [f"rom4_comparison_count: address={address:04x} count={count}" for address, count in counts.items()]
    for output, history, input_history in (
            ("1a48", "0084", "0080"), ("1a4a", "0088", "0084"),
            ("1a4c", "008c", "0088"), ("1a4e", "0080", "008c")):
        lines.append(f"rom4_filter_entry: st1=e900 bk=0010 ar0=0002 ar3={output} "
                     f"ar4={history} ar5=21a9 ar6={input_history} "
                     "coefficients=0387,11a3,2ad6,4000,4000,2ad6,11a3,0387")
    lines += ["rom4_comparison_input: reads=207040 nonzero=0 reduction_nonzero=0",
              "rom4_reduction_producer: pc=0f15 count=16",
              "rom4_reduction_producer: pc=3322 count=48264",
              "rom4_reduction_producer: pc=3323 count=48264"]
    for address, word, a in (("2400", "fc00", "0000000000"),
                             ("2402", "fc00", "0000000000"),
                             ("3385", "fa43", "0000000fa0")):
        lines += [f"rom4_comparison_fetch: t=2.1 address={address} word={word} "
                  f"pair94=0003d9de pair96=00000fa0 a={a} b=0000000000"] * 4
    return "\n".join(lines)


def scan_fixture():
    text = fixture().replace(processing_rf_summary(), rf_fixtures.C54xRom4RfBoundaryCheckTest().scan_summary())
    for old, new in [('6033', '1024'), ('48264', '8192'), ('6031', '1004'),
                     ('3017', '512'), ('207040', '648032'), ('st1=e900', 'st1=c900'),
                     ('address=33ac count=0', 'address=33ac count=2')]:
        text = text.replace(old, new)
    rows = ['rom4_comparison_count: address=332b count=20']
    for index in range(1, 21):
        flags = ['0838', '083a'][index - 1] if index <= 2 else ('0828' if index % 2 else '082a')
        candidate = {1: '0027', 2: '002d'}.get(index, '005a')
        rows.append(f'rom4_processing_transition: address=332b sequence={index} fd=0000 mode=0001 flags={flags} '
                    f'candidate={candidate} control2180=005a control21c2=f4cf a=0000000000 b=0000000000 t={1 + index * .01:.9f}')
        if index == 1:
            rows.append('rom4_processing_transition: address=33ac sequence=1 fd=0000 mode=0001 flags=083a '
                        'candidate=0027 control2180=005a control21c2=f4cf a=00002d0000 b=00005a0000 t=1.010001')
    rows += ['rom4_processing_transition: address=33ac sequence=2 fd=0001 mode=0001 flags=0832 '
             'candidate=0027 control2180=005a control21c2=f4cf a=00002d0000 b=00005a0000 t=6.4',
             'rom4_comparison_write: t=0.1 pc=0f10 address=06fd data=0000',
             'rom4_comparison_write: t=1.025 pc=312c address=06fd data=0000',
             'rom4_comparison_write: t=2.0 pc=3135 address=06fd data=0001']
    return text + '\n' + '\n'.join(rows)


class ProcessingCheckTest(unittest.TestCase):
    def test_accepts_separate_multi_entry_scan_contract(self):
        self.assertEqual(check(scan_fixture(), scan_30s=True), 1024)
        with self.assertRaises(ValueError):
            check(fixture(), scan_30s=True)
        with self.assertRaises(ValueError):
            check(scan_fixture())

    def test_scan_rejects_changed_selector_operands_counts_and_order(self):
        for old, new in [('address=332b count=20', 'address=332b count=19'),
                         ('address=32f4 count=1024', 'address=32f4 count=1023'),
                         ('address=3347 count=1004', 'address=3347 count=1005'),
                         ('fd=0000 mode=0001', 'fd=0001 mode=0001'),
                         ('candidate=002d', 'candidate=002e'),
                         ('a=00002d0000', 'a=00002d0001'),
                         ('control2180=005a', 'control2180=005b'),
                         ('st1=c900', 'st1=e900'),
                         ('pc=3135 address=06fd data=0001', 'pc=3134 address=06fd data=0001'),
                         ('t=1.025 pc=312c', 't=1.0 pc=312c'),
                         ('t=2.0 pc=3135', 't=7.0 pc=3135'),
                         ('t=6.4', 't=1.0')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                check(scan_fixture().replace(old, new), scan_30s=True)

    def test_scan_requires_complete_transition_capture(self):
        text = scan_fixture()
        line = next(line for line in text.splitlines() if 'rom4_processing_transition:' in line)
        for changed in (text.replace(line, ''), text + '\n' + line,
                        text.replace(line, line.replace('sequence=1', 'sequence=broken'))):
            with self.assertRaises(ValueError):
                check(changed, scan_30s=True)

    def test_accepts_observed_no_cell_pipeline(self):
        self.assertEqual(check(fixture()), 6033)

    def test_rejects_missing_observer(self):
        with self.assertRaisesRegex(ValueError, "mode-1"):
            check(summary())

    def test_rejects_filter_state_changes(self):
        for old, new in (("st1=e900", "st1=e940"), ("bk=0010", "bk=0008"),
                         ("ar0=0002", "ar0=0001"), ("ar4=0080", "ar4=0090"),
                         ("ar3=1a4e", "ar3=1a50"), ("ar5=21a9", "ar5=21aa"),
                         ("ar6=008c", "ar6=0080"), ("0387,11a3", "11a3,0387")):
            with self.subTest(change=new), self.assertRaisesRegex(ValueError, "filter entry"):
                check(fixture().replace(old, new))

    def test_rejects_missing_duplicate_or_reordered_filter_entries(self):
        lines = fixture().splitlines()
        entries = [line for line in lines if line.startswith("rom4_filter_entry:")]
        other = [line for line in lines if not line.startswith("rom4_filter_entry:")]
        for changed in (entries[:-1], entries + [entries[-1]], entries[::-1]):
            with self.assertRaisesRegex(ValueError, "filter entry"):
                check("\n".join(other + changed))

    def test_rejects_filter_loop_count_change(self):
        with self.assertRaisesRegex(ValueError, "filter loop"):
            check(fixture().replace("address=330a count=48264", "address=330a count=6033"))

    def test_rejects_changed_hook(self):
        with self.assertRaisesRegex(ValueError, "snapshot at 2400"):
            check(fixture().replace("address=2400 word=fc00", "address=2400 word=f495"))

    def test_rejects_unfilled_or_nonzero_buffer(self):
        for old, new in (("pc=3323 count=48264", "pc=3323 count=0"),
                         ("reduction_nonzero=0", "reduction_nonzero=1")):
            with self.assertRaises(ValueError):
                check(fixture().replace(old, new))

    def test_rejects_changed_full_width_operand(self):
        with self.assertRaisesRegex(ValueError, "snapshot"):
            check(fixture().replace("pair96=00000fa0", "pair96=00000000"))

    def test_rejects_new_continuation(self):
        with self.assertRaisesRegex(ValueError, "continuation"):
            check(fixture().replace("address=33ac count=0", "address=33ac count=1"))

    def test_rejects_duplicate_or_missing_count(self):
        with self.assertRaisesRegex(ValueError, "duplicate"):
            check(fixture() + "\nrom4_comparison_count: address=2400 count=6033")
        with self.assertRaisesRegex(ValueError, "hook count"):
            check(fixture().replace("rom4_comparison_count: address=2402 count=6033", ""))


if __name__ == "__main__":
    unittest.main()
