"""Executable checks of the small case harness, not C54x instruction semantics."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from tools.c54x_test_oracles import EXTRA_PROFILES
from tools.c54x_test_reviews import reviewed_profiles
from tools.run_c54x_core import check_result


ROOT = Path(__file__).resolve().parents[1]


class CaseFixtureTests(unittest.TestCase):
    def test_reviewed_case_identities_are_required_in_order(self):
        marker, ending = EXTRA_PROFILES['isa-square']
        cases = ['C54x ISA case: PASS id=' + name
                 for name in reviewed_profiles()['isa-square']['case_ids']]
        text = '\n'.join(cases + [marker, ending])
        check_result(text, 3, profile='isa-square')
        for changed in (cases[1:], cases + cases[:1], list(reversed(cases)),
                        ['C54x ISA case: PASS id=unknown'] + cases[1:]):
            with self.subTest(cases=changed), self.assertRaisesRegex(ValueError, 'identities'):
                check_result('\n'.join(changed + [marker, ending]), 3, profile='isa-square')

    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_case_helper_detects_state_and_bus_mismatches(self):
        source = r'''
#include <array>
#include <cstdint>
#include <stdexcept>
using u16 = std::uint16_t;
using u64 = std::uint64_t;
enum { AS_PROGRAM, AS_DATA };
struct emu_fatalerror : std::runtime_error {
    template<typename... Args> emu_fatalerror(const char *s, Args...) : std::runtime_error(s) { }
};
#include "tests/c54x/tms320c54x_test_case.h"
struct memory {
    std::array<u16, 16> words{};
    void write_word(unsigned address, u16 value) { words.at(address) = value; }
};
struct cpu {
    std::array<memory, 2> spaces{};
    std::array<u64, 4> registers{};
    memory &space(unsigned index) { return spaces.at(index); }
    void set_state_int(int index, u64 value) { registers.at(index) = value; }
    u64 state_int(int index) { return registers.at(index); }
};
int main() {
    cpu c;
    c54x_test::instruction_case test{"fixture", 1, {{2, 3}}, {{4, 5}},
        {{1, 0x1ff}}, {{1, 0xff, 0xff}}, 2, 3};
    c54x_test::prepare_case(c, test);
    if (c.spaces[AS_PROGRAM].words[2] != 3 || c.spaces[AS_DATA].words[4] != 5 ||
        c.registers[1] != 0x1ff) return 1;
    c54x_test::check_case(c, test, 2, 3); // Bits outside the mask are deliberately ignored.
    unsigned failures = 0;
    c.registers[1] = 0xfe;
    try { c54x_test::check_case(c, test, 2, 3); } catch (emu_fatalerror const &) { ++failures; }
    c.registers[1] = 0xff;
    try { c54x_test::check_case(c, test, 1, 3); } catch (emu_fatalerror const &) { ++failures; }
    try { c54x_test::check_case(c, test, 2, 4); } catch (emu_fatalerror const &) { ++failures; }
    return failures != 3;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'fixture.cpp').write_text(source)
            subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I', str(ROOT),
                            str(root / 'fixture.cpp'), '-o', str(root / 'fixture')],
                           check=True, capture_output=True)
            subprocess.run([str(root / 'fixture')], check=True, capture_output=True)
