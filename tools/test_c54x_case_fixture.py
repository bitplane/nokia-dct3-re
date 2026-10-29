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
    def test_ordered_data_events_reject_mismatch_and_rewind_at_checkpoint(self):
        header = (ROOT / 'tests/c54x/tms320c54x_test_case.h').read_text()
        begin = header.index('struct clocked_data_event')
        end = header.index('struct clocked_register_case', begin)
        source = r'''
#include <cassert>
#include <cstdint>
#include <vector>
using u16 = std::uint16_t; using u32 = std::uint32_t;
''' + header[begin:end] + r'''
int main() {
    std::vector<clocked_data_event> events{{false, 0x10, 0xabcd, 5}, {true, 0x20, 0x1234, 7}};
    unsigned cursor = 0;
    for (auto wrong : std::vector<clocked_data_event>{
        {true, 0x10, 0xabcd, 5}, {false, 0x11, 0xabcd, 5},
        {false, 0x10, 0xabcc, 5}, {false, 0x10, 0xabcd, 6}, events[1]}) {
        assert(!accept_data_event(events, cursor, wrong) && cursor == 0);
    }
    unsigned const before_read = cursor;
    assert(accept_data_event(events, cursor, events[0]) && cursor == 1);
    unsigned const after_read = cursor;
    assert(accept_data_event(events, cursor, events[1]) && cursor == 2);
    assert(!accept_data_event(events, cursor, events[1]) && cursor == 2);
    cursor = before_read;
    assert(accept_data_event(events, cursor, events[0]));
    cursor = after_read;
    assert(!accept_data_event(events, cursor, events[0]) && cursor == 1);
    assert(accept_data_event(events, cursor, events[1]) && cursor == events.size());
    cursor = 0;
    assert(!accept_data_event({}, cursor, events[0]) && cursor == 0);
}
'''
        with compiled_cpp(source) as program:
            subprocess.run([str(program)], check=True, capture_output=True)

    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_dual_schedules_keep_address_banks_and_numeric_inputs(self):
        source = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <tuple>
#include <vector>
using u16 = std::uint16_t; using u32 = std::uint32_t; using u64 = std::uint64_t;
using s16 = std::int16_t; using s32 = std::int32_t; using s64 = std::int64_t;
constexpr bool BIT(u64 value, unsigned bit) { return (value >> bit) & 1; }
#include "tests/c54x/tms320c54x_test_dual_input.h"
#include "tests/c54x/tms320c54x_test_dual_schedule.h"
#include "tests/c54x/tms320c54x_test_support.h"
auto state(c54x_test::dual_multiply_fixture const &f) {
    return std::tie(f.x, f.y, f.destination, f.flags, f.source, f.operation, f.linear, f.base,
        f.initial, f.final, f.xaddress, f.yaddress, f.xword, f.yword, f.st0, f.st1, f.pmst,
        f.result_st0, f.stored, f.result);
}
template<class s> void check(unsigned blocks, unsigned numeric, bool alu = false, bool macsu = false) {
    c54x_test::ordered_case_digest digest;
    unsigned const old_span = 18432 + 3 * numeric;
    std::vector<unsigned> seen(blocks * old_span);
    for (unsigned family = 0; family < blocks; ++family) {
        std::vector<std::array<unsigned, 3>> counts(numeric / 64);
        for (unsigned input = 0; input < s::per_family; ++input) {
            unsigned const key = s::key(family * s::per_family + input);
            digest.add(key);
            assert(key < seen.size() && !seen[key]++ && key / old_span == family);
            if (input < 18432) { assert(key == family * old_span + input); continue; }
            unsigned const relative = key - family * old_span - 18432;
            assert(relative % numeric == input - 18432);
            ++counts[(input - 18432) / 64][relative / numeric];
            // Small-family numeric copies must differ only in replay point.
            if (blocks < 8) {
                unsigned const raw = family * old_span + input;
                auto const first = c54x_test::dual_multiply_case(raw, false, alu, macsu);
                assert(first.snapshot == 0);
                for (unsigned snap = 1; snap < 3; ++snap) {
                    auto const other = c54x_test::dual_multiply_case(raw + snap * numeric, false, alu, macsu);
                    assert(other.snapshot == snap && state(first) == state(other));
                    if (macsu) assert(c54x_test::dual_macsu_seed(raw, false) ==
                        c54x_test::dual_macsu_seed(raw + snap * numeric, false));
                }
            }
        }
        for (auto c : counts) {
            std::sort(c.begin(), c.end());
            assert((c == std::array<unsigned, 3>{21, 21, 22}));
        }
    }
    assert(digest.count == s::cases);
    std::printf("cases=%u digest=%016llx\n", digest.count, (unsigned long long)digest.value);
}
int main() {
    check<c54x_test::dual_mpy_schedule>(1, 8192);
    check<c54x_test::dual_alu_schedule>(2, 8192, true);
    check<c54x_test::dual_macsu_schedule>(1, 65536, false, true);
    check<c54x_test::dual_mac_schedule>(8, 65536);
}
'''
        with compiled_cpp(source) as program:
            output = subprocess.check_output([str(program)], text=True).splitlines()
        self.assertEqual(output, ['cases=26624 digest=48c359ad090cdc45',
                                 'cases=53248 digest=0e097cb7d60a0ad8',
                                 'cases=83968 digest=a57c305fd5b3a3ba',
                                 'cases=671744 digest=a992a2f6ed8d9f42'])

    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_smem_schedule_keeps_every_input_and_balances_replay(self):
        source = r'''
#include <array>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>
#include "tests/c54x/tms320c54x_test_smem_schedule.h"
#include "tests/c54x/tms320c54x_test_support.h"
int main() {
    using namespace c54x_test;
    using s = smem_schedule;
    struct bank { s::kind kind; unsigned width, span; };
    for (auto b : {bank{s::kind::test_compare, 144, 18432}, {s::kind::signed_word, 576, 36864},
                   {s::kind::unsigned_add, 288, 18432}, {s::kind::unsigned_sub, 288, 18432},
                   {s::kind::shift16, 1152, 73728}}) {
        std::vector<unsigned> seen(b.span);
        std::vector<std::array<unsigned, 3>> groups(b.span / 8);
        ordered_case_digest coverage;
        for (unsigned raw = 0; raw != b.span; ++raw) {
            if (!s::valid(raw, b.kind)) continue;
            unsigned const key = s::key(raw, b.kind);
            assert(key < b.span * 3 && key % b.span == raw && !seen[raw]++);
            unsigned const group = raw % b.width + b.width * (raw / (b.width * 8));
            ++groups[group][key / b.span];
            coverage.add(key);
        }
        for (unsigned raw = 0; raw < b.span; ++raw) assert(seen[raw] == unsigned(raw % 18 != 5));
        for (unsigned i = 0; i < groups.size(); ++i) {
            auto counts = groups[i];
            std::sort(counts.begin(), counts.end());
            assert((counts == (i % 18 == 5 ? std::array<unsigned, 3>{0, 0, 0} :
                                            std::array<unsigned, 3>{2, 3, 3})));
        }
        assert(!s::valid(b.span, b.kind) && coverage.count == s::cases(b.kind));
        std::printf("cases=%u digest=%016llx\n", coverage.count, (unsigned long long)coverage.value);
    }
}
'''
        with compiled_cpp(source) as program:
            output = subprocess.check_output([str(program)], text=True).splitlines()
        self.assertEqual(output, ['cases=17408 digest=e8a1e8427edb11e5',
                                 'cases=34816 digest=7605a400a87ea30d',
                                 'cases=17408 digest=c21de3d0686da5d5',
                                 'cases=17408 digest=c21de3d0686da5d5',
                                 'cases=69632 digest=ecba0abea16a63e2'])

    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_mapped_mac_schedule_preserves_alias_inputs_and_balances_checkpoints(self):
        source = r'''
#include <array>
#include <cassert>
#include <cstdio>
#include <algorithm>
#include "tests/c54x/tms320c54x_test_memory_immediate_schedule.h"
#include "tests/c54x/tms320c54x_test_support.h"
int main() {
    using namespace c54x_test;
    std::array<unsigned, 262144> seen{};
    std::array<std::array<unsigned, 3>, 8192> groups{};
    ordered_case_digest coverage;
    for (unsigned i = 0; i < mapped_mac_inputs; ++i) {
        unsigned const key = mapped_mac_key(i);
        assert(key < 786432);
        unsigned const input = key % 262144, checkpoint = key / 262144;
        assert(input == i && !seen[input]++);
        unsigned const group = input % 64 + 64 * (input / (64 * 32));
        ++groups[group][checkpoint];
        coverage.add(key);
    }
    for (auto n : seen) assert(n == 1);
    for (auto group : groups) {
        std::sort(group.begin(), group.end());
        assert((group == std::array<unsigned, 3>{10, 11, 11}));
    }
    std::printf("cases=%u digest=%016llx\n", coverage.count, (unsigned long long)coverage.value);
}
'''
        with compiled_cpp(source) as program:
            output = subprocess.check_output([str(program)], text=True).strip()
        self.assertEqual(output, 'cases=262144 digest=020c459448602a55')

    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_memory_immediate_schedule_preserves_inputs_and_balances_checkpoints(self):
        source = r'''
#include <array>
#include <cassert>
#include <cstdio>
#include "tests/c54x/tms320c54x_test_memory_immediate_schedule.h"
#include "tests/c54x/tms320c54x_test_support.h"
int main() {
    using namespace c54x_test;
    ordered_case_digest coverage;
    for (unsigned mac = 0; mac != 2; ++mac) {
        unsigned const inputs = mac ? 65536 : 24576, forms = mac ? 8 : 12;
        unsigned const offset = mac ? 24576 : 0, old_offset = mac ? 98304 : 0;
        std::array<unsigned, 65536> seen{};
        std::array<std::array<unsigned, 4>, 1024> groups{};
        for (unsigned i = 0; i < inputs; ++i) {
            unsigned const key = memory_immediate_key(offset + i);
            assert(key >= old_offset && key < old_offset + inputs * 4);
            unsigned const input = (key - old_offset) % inputs;
            unsigned const checkpoint = (key - old_offset) / inputs;
            assert(input == i && !seen[input]++);
            unsigned const group = input % forms + forms * (input / (forms * 64));
            ++groups[group][checkpoint];
            coverage.add(key);
        }
        for (unsigned i = 0; i < inputs; ++i) assert(seen[i] == 1);
        for (unsigned group = 0; group < inputs / 64; ++group)
            for (unsigned checkpoint = 0; checkpoint < 4; ++checkpoint)
                assert(groups[group][checkpoint] == 16);
    }
    assert(coverage.count == 90112);
    std::printf("cases=%u digest=%016llx\n", coverage.count, (unsigned long long)coverage.value);
}
'''
        with compiled_cpp(source) as program:
            output = subprocess.check_output([str(program)], text=True).strip()
        self.assertEqual(output, 'cases=90112 digest=085b13667b310f01')

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
    saved.seekg(3);
    saved.setstate(std::ios::failbit | std::ios::eofbit);
    m.events.clear();
    if (c54x_test::restore_snapshot(m, saved) != 0 || m.loaded != "state" ||
        m.events != "load;") return 4; // Untimed overload must not cancel a timer.
    // Helpers must return serializer failures, not turn them into success.
    m.fail = true;
    if (c54x_test::capture_snapshot(m, saved) != 1 ||
        c54x_test::restore_snapshot(m, t, saved) != 1 ||
        c54x_test::restore_snapshot(m, saved) != 1) return 3;
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

    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_peripheral_rmw_schedule_preserves_all_inputs_and_saved_shapes(self):
        source = r'''
#include <iostream>
#include "tests/c54x/tms320c54x_test_support.h"
#include "tests/c54x/tms320c54x_test_rmw_schedule.h"
int main() {
    using schedule = c54x_test::peripheral_rmw_schedule;
    c54x_test::ordered_case_digest digest;
    for (unsigned v = 0; v < schedule::unique_cases; ++v) {
        unsigned const key = schedule::case_key(v);
        digest.add(key); std::cout << key << '\n';
    }
    std::cout << "digest=" << std::hex << digest.value << '\n';
}
'''
        with compiled_cpp(source) as program:
            output = subprocess.run([str(program)], check=True, capture_output=True,
                                    text=True).stdout.splitlines()
        expected, groups = [], defaultdict(Counter)
        for page in range(2):
            for pair in range(8):
                for flags in range(8):
                    for address in range(192):
                        for operation in range(3):
                            ordinal = operation + 3 * address + 576 * (flags + 8 * pair) + 36864 * page
                            point = (pair + operation + address) % 5
                            old = ordinal + point * 73728
                            expected.append(old)
                            groups[operation, address, flags, page][point] += 1
                            self.assertEqual(old % 73728, ordinal)
        self.assertEqual(list(map(int, output[:-1])), expected)
        self.assertEqual(len(set(expected)), 73728)
        self.assertTrue(all(0 <= v < 368640 for v in expected))
        self.assertEqual(len(groups), 9216)
        self.assertTrue(all(set(g) == set(range(5)) and sorted(g.values()) == [1, 1, 2, 2, 2]
                            for g in groups.values()))
        self.assertEqual(output[-1], 'digest=' + reviewed_profiles()['rmw-long-periph']['coverage']['fnv64'])

    def test_reviewed_case_identities_are_required_in_order(self):
        for profile, review in reviewed_profiles().items():
            if not review['case_ids']:
                continue
            markers, ending = EXTRA_PROFILES[profile]
            markers = [markers] if isinstance(markers, str) else list(markers)
            cases = ['C54x ISA case: PASS id=' + name
                     for name in reviewed_profiles()[profile]['case_ids']]
            text = '\n'.join(cases + markers + [ending])
            check_result(text, 3, profile=profile)
            for changed in (cases[1:], cases + cases[:1], list(reversed(cases)),
                            ['C54x ISA case: PASS id=unknown'] + cases[1:]):
                with self.subTest(profile=profile, cases=changed), self.assertRaisesRegex(ValueError, 'identities'):
                    check_result('\n'.join(changed + markers + [ending]), 3, profile=profile)

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
    if (!test.reset_before || !test.reset_initial.empty() || !test.reset_data.empty()) return 2;
    test.reset_before = false;
    test.reset_initial = {{2, 0xbeef}};
    test.reset_data = {{6, 0xbeef}};
    c.registers[2] = 0x1234;
    c.spaces[AS_DATA].words[6] = 0x1234;
    c54x_test::prepare_case(c, test);
    if (c.spaces[AS_PROGRAM].words[2] != 3 || c.spaces[AS_DATA].words[4] != 5 ||
        c.registers[1] != 0x1ff || c.registers[2] != 0x1234 ||
        c.spaces[AS_DATA].words[6] != 0x1234) return 1;
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
