"""Execute checkpoint selection; independently enumerate operand identities."""
from collections import defaultdict
import shutil
import subprocess
import unittest

from tools.c54x_test_reviews import reviewed_profiles
from tools.test_c54x_case_fixture import compiled_cpp


class ShiftedMemoryScheduleTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'host C++ compiler unavailable')
    def test_each_operand_once_with_balanced_stage_coverage(self):
        with compiled_cpp(r'''
#include "tests/c54x/tms320c54x_test_shifted_schedule.h"
#include <iostream>
int main() {
    using s = c54x_test::shifted_memory_schedule;
    for (bool bk : {false, true}) {
        unsigned count = 0;
        for (unsigned v = 0; v < (bk ? s::bk_end : s::end); v = s::next(v, bk)) {
            if (++count > (bk ? s::bk_cases : s::cases)) return 1;
            std::cout << bk << ' ' << v << ' ' << s::checkpoint_index(v, bk) << ' ' << s::case_key(v, bk) << '\n';
        }
        if (count != (bk ? s::bk_cases : s::cases)) return 2;
    }
}
''') as binary:
            actual = [tuple(map(int, line.split())) for line in
                      subprocess.check_output([str(binary)], text=True).splitlines()]

        expected, groups, digests = [], defaultdict(set), {}

        def fields(value, *radices):
            result = []
            for radix in radices:
                value, digit = divmod(value, radix)
                result.append(digit)
            self.assertEqual(value, 0)
            return result

        def bank(bk, first, size, kind, ordinal):
            digest = digests.get(bk, 14695981039346656037)
            for v in range(size):
                if kind == 'ram':
                    address, selector, pattern, flags, shift = fields(v, 12, 8, 4, 4, 32)
                    group = address + 12 * selector + 96 * flags + 384 * shift
                elif kind == 'ar':
                    register, selector, pattern, flags, shift = fields(v, 8, 8, 4, 4, 32)
                    group = register + 8 * selector + 64 * flags + 256 * shift
                elif kind == 'subc_ram':
                    address, destination, pattern, memory, flags = fields(v, 12, 2, 8, 8, 4)
                    group = address + 12 * destination + 24 * memory + 192 * flags
                elif kind == 'subc_ar':
                    register, destination, pattern, memory, flags = fields(v, 8, 2, 8, 8, 4)
                    group = register + 8 * destination + 16 * memory + 128 * flags
                else:
                    selector, pattern, flags, shift = fields(v, 8, 4, 4, 32)
                    group = selector + 8 * flags + 32 * shift
                checkpoint = (pattern + group) % 3
                key = (ordinal + v) * 3 + checkpoint
                expected.append((bk, first + v, checkpoint, key))
                groups[bk, first, group].add(checkpoint)
                for byte in key.to_bytes(4, 'little'):
                    digest = ((digest ^ byte) * 1099511628211) & ((1 << 64) - 1)
            digests[bk] = digest

        bank(0, 0, 49152, 'ram', 0)
        bank(0, 147456, 32768, 'ar', 49152)
        bank(0, 245760, 6144, 'subc_ram', 81920)
        bank(0, 264192, 4096, 'subc_ar', 88064)
        bank(1, 0, 4096, 'bk', 0)
        bank(1, 12288, 4096, 'bk', 4096)
        self.assertEqual(actual, expected)
        self.assertTrue(all(stages == {0, 1, 2} for stages in groups.values()))
        self.assertEqual(len({(bk, key) for bk, _, _, key in actual}), len(actual))
        for bk, profile in enumerate(('shifted-alu', 'shifted-bk')):
            with self.subTest(profile=profile):
                coverage = reviewed_profiles()[profile]['coverage']
                self.assertEqual(sum(row[0] == bk for row in actual), coverage['cases'])
                self.assertEqual(f'{digests[bk]:016x}', coverage['fnv64'])


if __name__ == '__main__':
    unittest.main()
