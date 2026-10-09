import unittest

from tools.c54x_rom4_processing_check import check
from tools.test_c54x_rom4_rf_boundary_check import summary


def fixture():
    frames = 6033
    counts = {0x3347: frames - 2, 0x3357: frames - 2,
              0x3382: 3017, 0x3385: 3017, 0x33a1: 0, 0x33a4: 0, 0x33ac: 0}
    counts.update({address: frames for address in (0x3360, 0x3362, 0x336c, 0x336e, 0x2400, 0x2402)})
    lines = [summary(), f"rom4_dispatch_count: address=32f4 count={frames}"]
    lines += [f"rom4_comparison_count: address={address:04x} count={count}" for address, count in counts.items()]
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


class ProcessingCheckTest(unittest.TestCase):
    def test_accepts_observed_no_cell_pipeline(self):
        self.assertEqual(check(fixture()), 6033)

    def test_rejects_missing_observer(self):
        with self.assertRaisesRegex(ValueError, "mode-1"):
            check(summary())

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
