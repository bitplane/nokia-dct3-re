"""Execute the actual header-only fixture selector without MAME or CPU code."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class C54xSelectionTests(unittest.TestCase):
    def test_atomic_restart_keeps_write_and_observer_order(self):
        from tools.test_c54x_case_fixture import compiled_cpp
        header = (ROOT / 'tests/c54x/tms320c54x_test_state.h').read_text()
        begin = header.index('\tbool next_atomic_phase(unsigned phase)')
        end = header.index('\tbool begin_atomic_case(', begin)
        source = r'''
#include <cassert>
#include <vector>
struct attotime { static int from_usec(int value) { return value; } };
struct cpu_regs { enum { STATE_PC, STATE_IDLE }; };
struct bench {
    std::vector<int> events;
    unsigned m_phase = 7;
    struct cpu {
        bench &owner;
        void set_state_int(int index, unsigned value) {
            assert(owner.m_phase == 7);
            owner.events.push_back(index); owner.events.push_back(value);
        }
    } core{*this}, *m_cpu = &core;
    struct timer {
        bench &owner;
        void adjust(int delay) {
            assert(owner.m_phase == 123);
            owner.events.push_back(delay);
        }
    } clock{*this}, *m_check_timer = &clock;
''' + header[begin:end] + r'''
};
int main() {
    bench b;
    assert(b.resume_atomic_at(0x5e0, 123));
    assert((b.events == std::vector<int>{cpu_regs::STATE_PC, 0x5e0, cpu_regs::STATE_IDLE, 0, 100}));
}
'''
        if not shutil.which('c++'):
            self.skipTest('C++ compiler unavailable')
        with compiled_cpp(source) as program:
            subprocess.run([str(program)], check=True, capture_output=True)

    def test_alias_order_missing_and_single_setup(self):
        compiler = os.environ.get('CXX', 'c++')
        if not shutil.which(compiler):
            self.skipTest('C++ compiler unavailable')
        source = r'''
#include "tests/c54x/tms320c54x_test_selection.h"
#include "tests/c54x/tms320c54x_test_support.h"
#include <stdexcept>
#include <vector>
struct bench {
    unsigned first = 0, second = 0, argument = 0;
    void a() { ++first; }
    void b() { ++second; }
    void defaults(unsigned value = 7) { argument = value; }
};
void require(bool value) { if (!value) throw std::runtime_error("selection"); }
struct cpu {
    std::vector<c54x_test::register_setting> writes;
    void set_state_int(int index, std::uint64_t value) { writes.emplace_back(index, value); }
};
struct space {
    std::vector<c54x_test::memory_word> writes;
    void write_word(std::uint32_t address, std::uint16_t value) { writes.emplace_back(address, value); }
};
int main() {
    bench test;
    for (auto name : {"primary", "alias"}) {
        require(c54x_test::start_profile(test, name, {
            {{"primary", "alias"}, [](bench &b) { b.a(); }},
            {{"alias"}, [](bench &b) { b.b(); }},
        }));
    }
    require(test.first == 2 && test.second == 0);
    require(!c54x_test::start_profile(test, "absent", {
        {{"primary", "alias"}, [](bench &b) { b.a(); }},
        {{"second"}, [](bench &b) { b.b(); }},
    }));
    require(test.first == 2 && test.second == 0);
    require(c54x_test::start_profile(test, "second", {
        {{"primary", "alias"}, [](bench &b) { b.a(); }},
        {{"second"}, [](bench &b) { b.b(); }},
    }));
    require(test.first == 2 && test.second == 1);
    require(!c54x_test::start_profile(test, "", {}));
    require(c54x_test::start_profile(test, "default", {
        {{"default"}, [](bench &b) { b.defaults(); }},
    }));
    require(test.argument == 7);
    cpu registers;
    unsigned selected = 3;
    c54x_test::set_registers(registers, {{4, 1}, {4, -1}, {-1, 0x8000000000ULL}, {4U + selected, 2}});
    require(registers.writes.size() == 4);
    space words;
    c54x_test::write_words(words, {{0x600, 0x1234}, {0x600, -1}, {0x7fffff, 0x10001}});
    require(words.writes.size() == 3);
    require(words.writes[0].address == 0x600 && words.writes[0].value == 0x1234);
    require(words.writes[1].address == 0x600 && words.writes[1].value == 0xffff);
    require(words.writes[2].address == 0x7fffff && words.writes[2].value == 1);
    require(registers.writes[0].index == 4 && registers.writes[0].value == 1);
    require(registers.writes[1].index == 4 && registers.writes[1].value == UINT64_MAX);
    require(registers.writes[2].index == -1 && registers.writes[2].value == 0x8000000000ULL);
    require(registers.writes[3].index == 7 && registers.writes[3].value == 2);
    c54x_test::set_registers(registers, {});
    require(registers.writes.size() == 4);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / 'test.cpp').write_text(source)
            subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror',
                            '-I', str(ROOT), str(path / 'test.cpp'), '-o', str(path / 'test')],
                           check=True, capture_output=True, text=True)
            subprocess.run([str(path / 'test')], check=True, capture_output=True, text=True)


if __name__ == '__main__':
    unittest.main()
