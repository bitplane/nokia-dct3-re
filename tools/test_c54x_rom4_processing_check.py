import functools
import unittest

from tools.c54x_rom4_processing_check import check, FIELDS, WIDTHS, WORDS, COEFFICIENTS
from tools import test_c54x_rom4_rf_boundary_check as rf_fixtures


@functools.cache
def fixture():
    """Synthetic ordered execution, not a copy of a handset trace."""
    lines = [rf_fixtures.C54xRom4RfBoundaryCheckTest().scan_summary(),
             'rom4_processing_observer: schema=1 phase=instruction_entry observer_writes=0',
             'rom4_comparison_write: t=0.1 pc=0f10 address=06fd data=0000']
    comparisons = set(range(510)) | {1021, 1023, 1025, 1026}
    continuations = {0: (0, 0x83a, 0x27), 2: (0, 0x83a, 0x2d),
                     1023: (1, 0x832, 0x27), 1025: (1, 0x832, 0x2d)}
    for index in range(1028):
        if index in (2, 22):
            lines.append(f'rom4_comparison_write: t=1.0 pc=312c address=06fd data={int(index == 22):04x}')
        addresses = [0x32f4] + [0x330a] * 8
        addresses += [0x332b] if index < 22 else [0x3347, 0x3357]
        addresses += [0x3360, 0x2400, 0x3362, 0x336c, 0x2402, 0x336e]
        if index in comparisons:
            addresses += [0x3382, 0x3385]
        if index in continuations:
            addresses.append(0x33ac)
        iteration = 0
        for address in addresses:
            row = dict.fromkeys(FIELDS, 0)
            row.update(address=address, word=WORDS[address], fd=int(index >= 22), mode=1,
                       control2180=0x5a, control21c2=0xf4cf, pair94=0x3d9de, pair96=0xfa0)
            if address == 0x330a:
                row.update(st1=0xc900, bk=0x10, ar0=2, ar3=0x1a48 + 2 * iteration,
                           ar4=0x80 + ((7 + 4 * iteration) & 15), ar5=0x21aa,
                           ar6=0x80 + ((1 + 4 * iteration) & 15))
                iteration += 1
            if address == 0x3385:
                row['a'] = 0xfa0
            if address == 0x33ac:
                row['fd'], row['flags'], row['candidate'] = continuations[index]
                row.update(a=(row['candidate'] + 6) << 16, b=0x5a0000)
            lines.append('rom4_processing_execute: ' + ' '.join(
                f'{name}={row[name]:0{width}x}' for name, width in zip(FIELDS, WIDTHS)))
            if address == 0x330a:
                lines.append('rom4_processing_coefficients: words=' + COEFFICIENTS)
    lines.append('rom4_comparison_input: reads=647872 nonzero=0 reduction_nonzero=0')
    return '\n'.join(lines)


class ProcessingCheckTest(unittest.TestCase):
    def test_complete_executed_pipeline(self):
        self.assertEqual(check(fixture(), scan_30s=True), 1028)

    def test_fetch_only_or_wrong_observer_cannot_pass(self):
        for old, new in [('phase=instruction_entry', 'phase=fetch'),
                         ('rom4_processing_execute:', 'rom4_comparison_fetch:')]:
            with self.assertRaises(ValueError):
                check(fixture().replace(old, new), scan_30s=True)
        with self.assertRaises(ValueError):
            check(fixture())

    def test_corrupt_execution_data_fails(self):
        for old, new in [('address=330a word=74d6', 'address=330a word=74d4'),
                         ('mode=0001', 'mode=0000'), ('pair96=00000fa0', 'pair96=00000000'),
                         ('ar5=21aa', 'ar5=21a9'), ('st1=c900', 'st1=e900'),
                         ('a=0000000fa0', 'a=0000000000'), ('a=00002d0000', 'a=00002d0001'),
                         (COEFFICIENTS, COEFFICIENTS.replace('0387', '0388', 1)),
                         ('reduction_nonzero=0', 'reduction_nonzero=1')]:
            with self.subTest(change=new), self.assertRaises(ValueError):
                check(fixture().replace(old, new, 1), scan_30s=True)

    def test_missing_duplicate_malformed_and_reordered_entries_fail(self):
        lines = fixture().splitlines()
        index = next(i for i, line in enumerate(lines) if 'address=3362 word=' in line)
        for replacement in ([], [lines[index]] * 2, [lines[index].replace('word=7713', 'word=oops')],
                            [lines[index].replace('address=3362', 'address=336e')]):
            with self.subTest(replacement=replacement), self.assertRaises(ValueError):
                check('\n'.join(lines[:index] + replacement + lines[index + 1:]), scan_30s=True)

    def test_missing_frame_and_selector_lifecycle_fail(self):
        text = fixture()
        first = next(line for line in text.splitlines() if 'address=32f4 word=' in line)
        for changed in (text.replace(first, '', 1),
                        text.replace('address=06fd data=0001', 'address=06fd data=0000'),
                        text.replace('fd=0000 mode=0001', 'fd=0001 mode=0001', 1)):
            with self.assertRaises(ValueError):
                check(changed, scan_30s=True)


if __name__ == '__main__':
    unittest.main()
