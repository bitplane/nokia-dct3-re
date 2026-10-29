"""Protect the independent instruction census, not the CPU's predicates."""
from pathlib import Path
import re
import shutil
import subprocess
import unittest

from tools.c54x_test_reviews import reviewed_profiles
from tools.test_c54x_case_fixture import compiled_cpp


class OperandDomainInventoryTest(unittest.TestCase):
    def test_census_has_every_reviewed_read_rmw_class_and_extension(self):
        source = (Path(__file__).resolve().parents[1] /
                  'tests/c54x/tms320c54x_test_domain_schedule.h').read_text()
        table = source.split('source_domains[] = {', 1)[1].split('};', 1)[0]
        actual = [(int(word, 16), int(operation, 16)) for word, operation in
                  re.findall(r'\{\s*(0x[0-9a-f]+),\s*(0x[0-9a-f]+|0)\s*\}', table)]
        # SPRU172C's implemented Smem source/RMW high-byte classes. MMR and
        # shifted-operation extension fields are separate operand grammars.
        scalar = list(range(0x00, 0x48))
        scalar += [0x4b, 0x4c, 0x4d] + list(range(0x50, 0x60))
        scalar += list(range(0x60, 0x6c)) + [0x71, 0x75, 0x78, 0x79, 0x7a, 0x7b, 0x7d, 0x7f]
        shifted = [0x0c40, 0x0d40, 0x0c00, 0x0d00, 0x0e00, 0x0f00,
                   0x0c20, 0x0d20, 0x0e20, 0x0f20]
        expected = [(value << 8, 0) for value in scalar]
        expected += [(0x6f00, operation) for operation in shifted]
        expected += [(value << 8, 0) for value in (0x48, 0x49, 0x4a, 0x73)]
        self.assertEqual(actual, expected)
        self.assertEqual(len(set(actual)), 125)
        self.assertEqual(len(scalar), 111)
        self.assertNotIn((0x6f00, 0x0c60), actual)  # STL's encoded operand is a destination.
        rows = []

        def append(opcode, operation, aliases, st0=0x1804, st1=0x0800, prefix=0, tag=0):
            for context in range(16):
                for mode in aliases:
                    key = ((opcode | mode) << 16) | (operation << 4) | context | tag
                    rows.append((key, opcode, operation, mode, context, st0, st1, prefix, 0xffff, 0))

        for opcode, operation in expected:
            append(opcode, operation, (*range(0x98, 0xa0), *range(0xf9, 0x100)))
        for opcode in (0x6800, 0x6900, 0x6a00, 0x6b00):
            append(opcode, 0, range(0xf9, 0x100), st0=0x0804, tag=0x8000)
        for operation in shifted:
            for shift in (1, 15, 16, 31):
                append(0x6f00, operation | shift, range(0x98, 0xa0))
        for opcode in (*range(0x2000, 0x3c00, 0x200), 0x6400, 0x6600, 0x4600):
            for destination in (0, 0x100):
                append(opcode | destination, 0, range(0x98, 0xa0), st1=0x0815, tag=0x4000)
        for opcode in (0x5000, 0x5200, 0x5400, 0x5800, 0x5a00, 0x5c00, 0x5e00):
            for destination in (0, 0x100):
                append(opcode | destination, 0, range(0x98, 0xa0), st1=0x0880, tag=0x2000)
        for opcode in (0x1e00, 0x1f00):
            append(opcode, 0, range(0x98, 0xa0), prefix=0xec03, tag=0x1000)
        for opcode in (0x6000, 0x6100, 0x1800, 0x1900, 0x1a00, 0x1b00, 0x1c00, 0x1d00,
                       0x7100, 0x7300, 0x7500, 0x7f00, 0x7d00, 0x7900, 0x7b00, 0x4c00, 0x4d00,
                       0x6800, 0x6900, 0x6a00, 0x6b00):
            append(opcode, 0, range(0x98, 0xa0), st0=0x0804, tag=0x8000)
        for snapshot in range(2):
            for page in range(2):
                for selectors in range(4):
                    for form in range(88):
                        absolute = form < 80
                        address = form if form < 8 else form + 16
                        opcode, mode = 0x6400 | (selectors << 8), 0xf8 if absolute else 0x98 + form - 80
                        context = (page << 1) | (snapshot << 3)
                        operation, tag = (0, 0) if absolute else (9, 0x0400)
                        key = ((opcode | mode) << 16) | ((address if absolute else operation) << 4) | tag | context
                        rows.append((key, opcode, operation, mode, context, 0x1028, 0x0800, 0,
                                     address if absolute else 0xffff, 5 if snapshot else 3))
        rows = [row + (0xffff,) for row in rows]
        # Retired mapped probes: ADDC, ADDS/SUBS, shifted ADD/SUB, logical
        # memory operations, and MVDM's six accumulator slices. The source
        # lists are the old fixtures' explicit unowned-address sets.
        groups = [(opcode, 80, False) for opcode in
                  (0x0600, 0x0700, 0x0200, 0x0300, 0x0a00, 0x0b00,
                   *range(0x3c00, 0x4400, 0x100))]
        groups += [(opcode, 16, False) for opcode in range(0x1800, 0x1e00, 0x100)]
        groups += [(opcode, 86, True) for opcode in range(0x7208, 0x720e)]
        for group_index, (opcode, sources, move) in enumerate(groups):
            for page in range(2):
                for form in range(1 if move else 3):
                    for index in range(sources):
                        address = index if index < (16 if move else 8) else index + (10 if move else 16)
                        point = (index + form + page + group_index) % 2
                        context = (page << 1) | (point << 3)
                        mode = 0 if move else address if form == 0 else 0xf8 if form == 1 else 0x83
                        absolute = address if move or form == 1 else 0xffff
                        indirect = address if not move and form == 2 else 0xffff
                        operand = address if absolute != 0xffff or indirect != 0xffff else 0
                        key = ((opcode | mode) << 16) | (operand << 4) | context
                        rows.append((key, opcode, 0, mode, context, 0x1800, 0x900 if move else 0x800,
                                     0, absolute, (4 if move or form == 1 else 3) + point, indirect))
        rows = [row + (0, 0x1400, 0) for row in rows]
        prefix_count = len(rows)
        for repeated, families, modes in (
                (False, (0x4800, 0x4900, 0x4a00, 0x7200, 0x7300, 0x7700, 0x8800, 0x8900, 0x8a00), 32),
                (True, (0x7000, 0x7100, 0x7c00, 0x7d00, 0x7e00, 0x7f00), 25)):
            span = 4800 if repeated else 2304
            points = {}
            for frontend in range(2):
                for page in range(2):
                    for pmst in range(2):
                        for long_count in range(2 if repeated else 1):
                            for count_index in range(2 if repeated else 1):
                                for family, opcode in enumerate(families):
                                    for mode in range(modes):
                                        raw = mode + modes * family + (150 * count_index + 300 * long_count +
                                              600 * pmst + 1200 * page + 2400 * frontend if repeated else
                                              288 * pmst + 576 * page + 1152 * frontend)
                                        point = (mode + family + pmst + page + frontend + count_index + long_count) % 2
                                        points[(family, point)] = points.get((family, point), 0) + 1
                                        prefix = (0xf070 if long_count else 0xec00 | (2 * count_index)) if repeated else 0
                                        words = ([prefix, 2 * count_index] if long_count else [prefix]) if repeated else []
                                        words += [opcode | (0xe0 + mode), 0x900] + ([0xa00] if repeated else []) + [0xf4e1]
                                        key = (0xc0000000 if repeated else 0x80000000) | (raw + point * span)
                                        rows.append((key, opcode, 0, 0xe0 + mode, (page << 1) | (frontend << 2) | (point << 3),
                                                     0x1804, 0x800, prefix, 0xffff, 12 if point else 1, 0xffff,
                                                     0x4000 if pmst else 0, 0xf040, len(words), *words))
            self.assertEqual(sum(points.values()), span)
            for family in range(len(families)):
                self.assertEqual(points[(family, 0)], points[(family, 1)])
        if not shutil.which('c++'):
            self.skipTest('host C++ compiler unavailable')
        with compiled_cpp(r'''
#include "tests/c54x/tms320c54x_test_domain_schedule.h"
#include <iostream>
int main() {
    for (unsigned i = 0; i < c54x_test::domain_total_cases; ++i) {
        auto const c = c54x_test::select_domain(i);
        std::cout << c.key() << ' ' << c.instruction.opcode << ' ' << c.instruction.operation
            << ' ' << c.mode << ' ' << c.context << ' ' << c.st0 << ' ' << c.st1 << ' ' << c.prefix
            << ' ' << c.absolute_source << ' ' << c.checkpoint << ' ' << c.indirect_source
            << ' ' << c.pmst << ' ' << c.ar_base << ' ' << c.words.size();
        for (auto word : c.words) std::cout << ' ' << word;
        std::cout << '\n';
    }
    try { c54x_test::select_domain(c54x_test::domain_total_cases); }
    catch (std::out_of_range const &) { return 0; }
    return 1;
}
''') as binary:
            actual_rows = [tuple(map(int, line.split())) for line in
                           subprocess.check_output([str(binary)], text=True).splitlines()]
        self.assertEqual(actual_rows, rows)
        # These replace the deleted one-off DLD/LDM/MPY/MAC admission probes.
        # Every base context must still exercise each exact instruction word.
        for word in (0x569b, 0x489a, 0x629b, 0x649b):
            contexts = [row[4] for row in actual_rows[:30000]
                        if row[1] | row[3] == word]
            self.assertEqual(contexts, list(range(16)), f'{word:04x}')
        keys = [row[0] for row in rows]
        self.assertEqual(len(keys), len(set(keys)))
        digest = 14695981039346656037
        for index, key in enumerate(keys):
            for byte in key.to_bytes(4, 'little'):
                digest = ((digest ^ byte) * 1099511628211) & ((1 << 64) - 1)
            if index == 30447:
                self.assertEqual(f'{digest:016x}', 'ba74a1da74928725')
            if index == 44655:
                self.assertEqual(f'{digest:016x}', '6ae942418d722665')
            if index == 46063:
                self.assertEqual(f'{digest:016x}', '60d443519ef671c5')
            if index == prefix_count - 1:
                self.assertEqual(f'{digest:016x}', '09bcb725ffd76435')
        coverage = reviewed_profiles()['source-domain']['coverage']
        self.assertEqual(len(keys), coverage['cases'])
        self.assertEqual(f'{digest:016x}', coverage['fnv64'])


if __name__ == '__main__':
    unittest.main()
