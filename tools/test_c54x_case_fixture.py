"""Executable checks of the small case harness, not C54x instruction semantics."""
from pathlib import Path
from collections import Counter, defaultdict
from contextlib import contextmanager
import shutil
import subprocess
import tempfile
import unittest

from tools.c54x_test_oracles import EXTRA_PROFILES
from tools.c54x_test_reviews import reviewed_profiles
from tools.run_c54x_core import check_result


ROOT = Path(__file__).resolve().parents[1]


@contextmanager
def compiled_cpp(source):
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        (root / 'fixture.cpp').write_text(source)
        program = root / 'fixture'
        subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I', str(ROOT),
                        str(root / 'fixture.cpp'), '-o', str(program)],
                       check=True, capture_output=True)
        yield program


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
        with compiled_cpp(source) as program:
            subprocess.run([str(program)], check=True, capture_output=True)

    def test_reviewed_ordered_coverage_is_required(self):
        for profile in ('rmw-long', 'long-mvkd', 'reg-banks', 'source-domain'):
            markers, ending = EXTRA_PROFILES[profile]
            markers = (markers,) if isinstance(markers, str) else markers
            coverage = reviewed_profiles()[profile]['coverage']
            marker = next(m for m in markers if m.startswith(coverage['result_prefix']))
            text = '\n'.join((*markers, ending))
            check_result(text, 3, profile=profile)
            digest = reviewed_profiles()[profile]['coverage']['fnv64']
            for changed in (text.replace(digest, '0' * 16),
                            text.replace('coverage_fnv64=' + digest, ''),
                            marker + '\n' + text):
                with self.subTest(profile=profile), self.assertRaisesRegex(ValueError, 'coverage digest'):
                    check_result(changed, 3, profile=profile)

    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_register_schedule_preserves_stimuli_and_all_producer_replays(self):
        source = r'''
#include <iostream>
#include "tests/c54x/tms320c54x_test_support.h"
#include "tests/c54x/tms320c54x_test_register_schedule.h"
int main() {
    using s = c54x_test::register_bank_schedule;
    c54x_test::ordered_case_digest digest;
    for (unsigned v = 0; v < s::end; v = s::next(v)) {
        unsigned const key = s::case_key(v);
        digest.add(key);
        std::cout << v << ' ' << key << '\n';
    }
    std::cout << "digest=" << std::hex << digest.value << '\n';
}
'''
        expected, groups, producers = [], defaultdict(Counter), defaultdict(Counter)
        for load in (False, True):
            operations, base = (1, 196896) if load else (2, 0)
            for page in range(2):
                for asm in range(2):
                    for flags in range(16 if load else 8):
                        for pattern in range(8):
                            for selector in range(4):
                                for shift in range(32):
                                    for op in range(operations):
                                        group = (op + operations * (shift + 32 * selector + 128 * flags)
                                                 + 2048 * asm + 4096 * page)
                                        checkpoint = (pattern + group) % 3
                                        variant = (base + op + operations * (shift + 32 * selector + 128 * pattern + 1024 * flags)
                                                   + 16384 * asm + 32768 * page)
                                        expected.append((variant, len(expected) * 3 + checkpoint))
                                        groups[load, op, page, asm, flags, selector, shift][checkpoint] += 1
            for checkpoint in range(3):
                for gap in range(3):
                    for pattern in range(4):
                        for selector in range(4):
                            for op in range(operations):
                                local = op + operations * (selector + 4 * pattern + 16 * gap)
                                variant = base + 196608 + checkpoint * (48 * operations) + local
                                expected.append((variant, len(expected) * 3 + checkpoint))
                                producers[load, gap, pattern, selector, op][checkpoint] += 1
        self.assertEqual(len(expected), 131504)
        self.assertEqual(len(groups), 16384)
        self.assertTrue(all(sorted(group.values()) == [2, 3, 3] for group in groups.values()))
        self.assertEqual(len(producers), 144)
        self.assertTrue(all(group == Counter({0: 1, 1: 1, 2: 1}) for group in producers.values()))
        with compiled_cpp(source) as program:
            output = subprocess.run([str(program)], check=True, capture_output=True,
                                    text=True).stdout.splitlines()
        self.assertEqual([tuple(map(int, line.split())) for line in output[:-1]], expected)
        self.assertEqual(output[-1], 'digest=' + reviewed_profiles()['reg-banks']['coverage']['fnv64'])

    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_store_schedule_retains_each_stimulus_and_checkpoint_group(self):
        source = r'''
#include <iostream>
#include "tests/c54x/tms320c54x_test_support.h"
#include "tests/c54x/tms320c54x_test_store_schedule.h"
int main() {
    using s = c54x_test::long_store_schedule;
    c54x_test::ordered_case_digest digest;
    for (unsigned v = 0; v < s::end; v = s::next(v)) {
        unsigned const key = s::case_key(v);
        digest.add(key);
        std::cout << v << ' ' << key << ' ' << s::checkpoint(v) << '\n';
    }
    std::cout << "digest=" << std::hex << digest.value << '\n';
}
'''
        expected, groups, ordinal = [], defaultdict(Counter), 0
        for shifted in (False, True):
            forms = 7 if shifted else 25
            for major in range(32 if shifted else 2):
                for pattern in range(4):
                    for flags in range(4):
                        for selector in range(4):
                            for offset in range(4):
                                for form in range(forms):
                                    checkpoint = (form + offset + selector + flags + pattern + major) % 3
                                    variant = 38400 + ordinal - 12800 if shifted else ordinal
                                    tick = ((4, 7, 8) if shifted else (3, 6, 7))[checkpoint]
                                    expected.append((variant, ordinal * 3 + checkpoint, tick))
                                    groups[shifted, major, flags, selector, offset, form][checkpoint] += 1
                                    ordinal += 1
        self.assertEqual(ordinal, 70144)
        self.assertEqual(len(groups), 3200 + 14336)
        self.assertTrue(all(sorted(group.values()) == [1, 1, 2] for group in groups.values()))
        with compiled_cpp(source) as program:
            output = subprocess.run([str(program)], check=True, capture_output=True,
                                    text=True).stdout.splitlines()
        self.assertEqual([tuple(map(int, line.split())) for line in output[:-1]], expected)
        self.assertEqual(output[-1], 'digest=' + reviewed_profiles()['long-mvkd']['coverage']['fnv64'])

    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_rmw_schedule_preserves_stimuli_and_checkpoint_shapes(self):
        source = r'''
#include <iostream>
#include "tests/c54x/tms320c54x_test_support.h"
#include "tests/c54x/tms320c54x_test_rmw_schedule.h"
int main() {
    using schedule = c54x_test::long_rmw_schedule;
    c54x_test::ordered_case_digest digest;
    for (unsigned v = 0; v < schedule::unique_cases; ++v) {
        unsigned const key = schedule::case_key(v);
        digest.add(key);
        std::cout << key << '\n';
    }
    std::cout << "digest=" << std::hex << digest.value << '\n';
}
'''
        with compiled_cpp(source) as program:
            output = subprocess.run([str(program)], check=True, capture_output=True,
                                    text=True).stdout.splitlines()
        expected, groups = [], defaultdict(Counter)
        for compatibility in range(2):
            for arithmetic in range(64):
                flags, pattern = arithmetic % 8, arithmetic // 8
                for address in range(100):
                    for operation in range(3):
                        ordinal = operation + 3 * address + 300 * arithmetic + 19200 * compatibility
                        checkpoint = (pattern + operation + address) % 4
                        expected.append(ordinal * 4 + checkpoint)
                        groups[operation, address, compatibility, flags][checkpoint] += 1
        self.assertEqual(list(map(int, output[:-1])), expected)
        self.assertEqual(len(groups), 4800)
        self.assertTrue(all(group == Counter({i: 2 for i in range(4)}) for group in groups.values()))
        self.assertEqual(output[-1], 'digest=' + reviewed_profiles()['rmw-long']['coverage']['fnv64'])

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
        with compiled_cpp(source) as program:
            subprocess.run([str(program)], check=True, capture_output=True)
