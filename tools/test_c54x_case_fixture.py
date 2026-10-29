"""Executable checks of the small case harness, not C54x instruction semantics."""
from pathlib import Path
from collections import Counter, defaultdict
import shutil
import subprocess
import tempfile
import unittest

from tools.c54x_test_oracles import EXTRA_PROFILES
from tools.c54x_test_reviews import reviewed_profiles
from tools.run_c54x_core import check_result


ROOT = Path(__file__).resolve().parents[1]


class CaseFixtureTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_snapshot_service_preserves_stream_and_timer_order(self):
        source = r'''
#include <sstream>
#include <string>
struct attotime { static constexpr int never = -1; };
#include "tests/c54x/tms320c54x_test_snapshot.h"
struct machine {
    std::string events, loaded;
    bool fail = false;
    machine &save() { return *this; }
    int write_stream(std::stringstream &s) {
        events += "save;";
        if (!s.str().empty() || !s.good()) return 2;
        s << "state";
        return fail ? 1 : 0;
    }
    int read_stream(std::stringstream &s) {
        events += "load;";
        if (s.tellg() != 0 || !s.good()) return 2;
        s >> loaded;
        return fail ? 1 : 0;
    }
};
struct timer {
    machine &owner;
    void adjust(int when) { owner.events += when == attotime::never ? "stop;" : "wrong;"; }
};
int main() {
    machine m;
    timer t{m};
    std::stringstream saved("stale state");
    saved.setstate(std::ios::failbit);
    if (c54x_test::capture_snapshot(m, saved) != 0 || saved.str() != "state") return 1;
    saved.seekg(3);
    saved.setstate(std::ios::eofbit | std::ios::failbit);
    if (c54x_test::restore_snapshot(m, t, saved) != 0 || m.loaded != "state" ||
        m.events != "save;stop;load;") return 2;
    // Helpers must return serializer failures, not turn them into success.
    m.fail = true;
    if (c54x_test::capture_snapshot(m, saved) != 1 ||
        c54x_test::restore_snapshot(m, t, saved) != 1) return 3;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'snapshot.cpp').write_text(source)
            subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I', str(ROOT),
                            str(root / 'snapshot.cpp'), '-o', str(root / 'snapshot')],
                           check=True, capture_output=True)
            subprocess.run([str(root / 'snapshot')], check=True, capture_output=True)

    def test_rmw_ordered_coverage_is_required(self):
        for profile in ('rmw-long', 'rmw-long-full'):
            marker, ending = EXTRA_PROFILES[profile]
            text = marker + '\n' + ending
            check_result(text, 3, profile=profile)
            digest = reviewed_profiles()[profile]['coverage']['fnv64']
            for changed in (text.replace(digest, '0' * 16),
                            text.replace('coverage_fnv64=' + digest, ''),
                            marker + '\n' + text):
                with self.subTest(profile=profile), self.assertRaisesRegex(ValueError, 'coverage digest'):
                    check_result(changed, 3, profile=profile)

    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_rmw_schedule_preserves_stimuli_and_checkpoint_shapes(self):
        source = r'''
#include <iostream>
#include "tests/c54x/tms320c54x_test_support.h"
#include "tests/c54x/tms320c54x_test_rmw_schedule.h"
int main(int argc, char **) {
    using schedule = c54x_test::long_rmw_schedule;
    bool const exhaustive = argc > 1;
    unsigned const count = exhaustive ? schedule::exhaustive_cases : schedule::unique_cases;
    c54x_test::ordered_case_digest digest;
    for (unsigned v = 0; v < count; ++v) {
        unsigned const key = schedule::case_key(v, exhaustive);
        digest.add(key);
        std::cout << key << '\n';
    }
    std::cout << "digest=" << std::hex << digest.value << '\n';
}
'''
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'schedule.cpp').write_text(source)
            subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I', str(ROOT),
                            str(root / 'schedule.cpp'), '-o', str(root / 'schedule')],
                           check=True, capture_output=True)
            for exhaustive in (False, True):
                output = subprocess.run([str(root / 'schedule')] + (['full'] if exhaustive else []),
                                        check=True, capture_output=True, text=True).stdout.splitlines()
                actual = list(map(int, output[:-1]))
                expected = []
                groups = defaultdict(Counter)
                for checkpoint in (range(4) if exhaustive else range(1)):
                    for compatibility in range(2):
                        for arithmetic in range(64):
                            flags, pattern = arithmetic % 8, arithmetic // 8
                            for address in range(100):
                                for operation in range(3):
                                    ordinal = operation + 3 * address + 300 * arithmetic + 19200 * compatibility
                                    selected = checkpoint if exhaustive else (pattern + operation + address) % 4
                                    expected.append(ordinal * 4 + selected)
                                    groups[operation, address, compatibility, flags][selected] += 1
                self.assertEqual(actual, expected)
                self.assertEqual(len(groups), 4800)
                self.assertTrue(all(group == Counter({i: 8 if exhaustive else 2 for i in range(4)})
                                    for group in groups.values()))
                profile = 'rmw-long-full' if exhaustive else 'rmw-long'
                self.assertEqual(output[-1], 'digest=' + reviewed_profiles()[profile]['coverage']['fnv64'])

    def test_reviewed_case_identities_are_required_in_order(self):
        for profile in ('isa-square', 'isa-parallel'):
            marker, ending = EXTRA_PROFILES[profile]
            cases = ['C54x ISA case: PASS id=' + name
                     for name in reviewed_profiles()[profile]['case_ids']]
            text = '\n'.join(cases + [marker, ending])
            check_result(text, 3, profile=profile)
            for changed in (cases[1:], cases + cases[:1], list(reversed(cases)),
                            ['C54x ISA case: PASS id=unknown'] + cases[1:]):
                with self.subTest(profile=profile, cases=changed), self.assertRaisesRegex(ValueError, 'identities'):
                    check_result('\n'.join(changed + [marker, ending]), 3, profile=profile)

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
    u16 read_word(unsigned address) { return words.at(address); }
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
        {{1, 0x1ff}}, {{1, 0xff, 0xff}}, 2, 3, {{4, 5}}};
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
    c.spaces[AS_DATA].words[4] = 6;
    try { c54x_test::check_case(c, test, 2, 3); } catch (emu_fatalerror const &) { ++failures; }
    return failures != 4;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'fixture.cpp').write_text(source)
            subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I', str(ROOT),
                            str(root / 'fixture.cpp'), '-o', str(root / 'fixture')],
                           check=True, capture_output=True)
            subprocess.run([str(root / 'fixture')], check=True, capture_output=True)
