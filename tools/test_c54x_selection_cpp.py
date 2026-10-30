"""Execute fixture selection and observer/replay services without MAME."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class C54xSelectionTests(unittest.TestCase):
    def test_selected_result_owner_precedes_colliding_legacy_phase(self):
        from tools.test_c54x_case_fixture import compiled_cpp
        source = (ROOT / 'tests/c54x/tms320c54x_test_dispatch.cpp').read_text()
        begin = source.index('\tif (m_result_owner)')
        end = source.index('\tauto &program', begin)
        cpp = r'''
#include <cassert>
#include <stdexcept>
struct bench {
    bool (bench::*m_result_owner)() = nullptr;
    unsigned phase = 62134, selected = 0, legacy = 0;
    bool owns_phase = true;
    bool result() { assert(phase == 62134); ++selected; return owns_phase; }
    void expect(bool valid, const char *) { if (!valid) throw std::runtime_error("unowned continuation"); }
    void tick() {
''' + source[begin:end] + r'''
        assert(phase == 62134); ++legacy;
    }
};
int main() {
    bench old; old.tick(); assert(old.legacy == 1 && !old.selected);
    bench scoped; scoped.m_result_owner = &bench::result;
    scoped.tick(); assert(scoped.selected == 1 && !scoped.legacy);
    scoped.owns_phase = false;
    try { scoped.tick(); assert(false); } catch (std::runtime_error const &) { }
    assert(scoped.selected == 2 && !scoped.legacy);
}
'''
        if not shutil.which('c++'):
            self.skipTest('C++ compiler unavailable')
        with compiled_cpp(cpp) as program:
            subprocess.run([str(program)], check=True, capture_output=True)

    def test_timed_restore_cancels_loads_then_suspends_before_continuation(self):
        from tools.test_c54x_case_fixture import compiled_cpp
        header = (ROOT / 'tests/c54x/tms320c54x_test_state.h').read_text()
        begin = header.index('\tbool restore_pipeline_at(')
        end = header.index('\tbool next_atomic_phase(', begin)
        source = r'''
#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>
struct attotime { static constexpr int never = -1; };
constexpr int STATERR_NONE = 0, SUSPEND_REASON_DISABLE = 1;
#include "tests/c54x/tms320c54x_test_snapshot.h"
using c54x_test::restore_snapshot;
struct bench {
    std::vector<int> events;
    unsigned m_phase = 9;
    bool fail_load = false;
    std::stringstream m_saved_pipeline{"saved"};
    bench &machine() { return *this; }
    bench &save() { return *this; }
    int read_stream(std::stringstream &stream) {
        assert(m_phase == 9 && stream.tellg() == 0);
        std::string text; stream >> text; assert(text == "saved");
        events.push_back(2); return fail_load ? 1 : 0;
    }
    void expect(bool ok, const char *message) {
        assert(std::string(message) == "retained state" && m_phase == 9);
        events.push_back(3); if (!ok) throw std::runtime_error(message);
    }
    struct cpu {
        bench &owner;
        void suspend(int reason, bool eat_clocks) {
            assert(reason == SUSPEND_REASON_DISABLE && eat_clocks && owner.m_phase == 9);
            owner.events.push_back(4);
        }
    } core{*this}, *m_cpu = &core;
    struct timer {
        bench &owner;
        void adjust(int when) { assert(when == attotime::never); owner.events.push_back(1); }
        void enable(bool enabled) {
            assert(enabled && owner.m_phase == 123); owner.events.push_back(5);
        }
    } clock{*this}, *m_check_timer = &clock;
''' + header[begin:end] + r'''
};
int main() {
    bench ok; ok.m_saved_pipeline.seekg(5); ok.m_saved_pipeline.setstate(std::ios::eofbit);
    assert(ok.restore_pipeline_at(123, "retained state"));
    assert((ok.events == std::vector<int>{1,2,3,4,5}));
    bench bad; bad.fail_load = true;
    try { bad.restore_pipeline_at(123, "retained state"); assert(false); }
    catch (std::runtime_error const &) { }
    assert(bad.m_phase == 9 && (bad.events == std::vector<int>{1,2,3}));
}
'''
        if not shutil.which('c++'):
            self.skipTest('C++ compiler unavailable')
        with compiled_cpp(source) as program:
            subprocess.run([str(program)], check=True, capture_output=True)

    def test_counted_checkpoint_rewinds_observers_before_guest_load(self):
        from tools.test_c54x_case_fixture import compiled_cpp
        header = (ROOT / 'tests/c54x/tms320c54x_test_state.h').read_text()
        begin = header.index('\tvoid capture_pipeline_at(')
        end = header.index('\tbool next_atomic_phase(', begin)
        source = r'''
#include <array>
#include <cassert>
#include <initializer_list>
#include <stdexcept>
#include <vector>
struct attotime { static constexpr int never = -1; };
constexpr int STATERR_NONE=0, SUSPEND_REASON_DISABLE=1;
#include "tests/c54x/tms320c54x_test_snapshot.h"
using c54x_test::capture_snapshot; using c54x_test::restore_snapshot;
struct bench {
    unsigned reads=3, writes=7, fetches=11, m_idle_fixture_tick=4, m_phase=9;
    unsigned m_pipeline_counter_count=0;
    bool m_pipeline_replay=false, fail_save=false, fail_load=false;
    std::array<unsigned,9> m_recurring_irq_snapshot{};
    std::array<unsigned*,9> m_pipeline_counter_bindings{};
    std::stringstream m_saved_pipeline{"stale"};
    std::vector<unsigned> events;
    bench &machine() { return *this; }
    bench &save() { return *this; }
    void expect(bool valid, const char *message) { if(!valid) throw std::runtime_error(message); }
    int write_stream(std::stringstream &out) {
        assert(m_phase==9 && m_idle_fixture_tick==5 && !m_pipeline_replay);
        assert(m_pipeline_counter_count==3 && m_recurring_irq_snapshot[0]==3 &&
            m_recurring_irq_snapshot[1]==7 && m_recurring_irq_snapshot[2]==11);
        for(unsigned i=3;i<9;++i) assert(m_recurring_irq_snapshot[i]==0);
        assert(out.str().empty()); out<<"saved"; events.push_back(1); return fail_save;
    }
    int read_stream(std::stringstream &in) {
        assert(m_phase==9 && m_idle_fixture_tick==5 && m_pipeline_replay);
        assert(reads==3 && writes==7 && fetches==11);
        std::string text; in>>text; assert(text=="saved"); events.push_back(3); return fail_load;
    }
    struct cpu {
        bench &b;
        void suspend(int reason,bool eat) { assert(reason==1 && eat && b.m_phase==9); b.events.push_back(4); }
    } core{*this}, *m_cpu=&core;
    struct timer {
        bench &b;
        void adjust(int when) { assert(when==attotime::never); b.events.push_back(2); }
        void enable(bool yes) { assert(yes && b.m_phase==123); b.events.push_back(5); }
    } clock{*this}, *m_check_timer=&clock;
HELPERS
    void capture() {
        m_recurring_irq_snapshot.fill(99); m_idle_fixture_tick=5;
        capture_pipeline_at(5,{&reads,&writes,&fetches},"capture");
    }
};
int main() {
    bench b;
    b.capture_pipeline_at(5,{nullptr},"not at checkpoint"); assert(b.events.empty());
    b.capture(); assert((b.events==std::vector<unsigned>{1}));
    b.reads=30; b.writes=70; b.fetches=110; b.m_idle_fixture_tick=12;
    assert(b.replay_pipeline_at(123,5,"restore"));
    assert((b.events==std::vector<unsigned>{1,2,3,4,5}));
    b.reads=99;
    b.capture_pipeline_at(5,{nullptr},"already replayed");
    assert(!b.replay_pipeline_at(456,6,"already replayed") && b.reads==99 && b.events.size()==5);
    bench bad; bad.capture(); bad.fail_load=true; bad.reads=bad.writes=bad.fetches=99;
    try { bad.replay_pipeline_at(123,5,"restore"); assert(false); } catch(std::runtime_error const &) { }
    assert(bad.m_phase==9 && (bad.events==std::vector<unsigned>{1,2,3}));
    bench no_save; no_save.fail_save=true;
    try { no_save.capture(); assert(false); } catch(std::runtime_error const &) { }
    assert(no_save.m_phase==9 && !no_save.m_pipeline_replay && no_save.events.size()==1);
    bench invalid; invalid.m_idle_fixture_tick=5;
    for(unsigned kind=0;kind<4;++kind) {
        try {
            if(kind==0) invalid.capture_pipeline_at(5,{},"empty");
            if(kind==1) invalid.capture_pipeline_at(5,{nullptr},"null");
            if(kind==2) invalid.capture_pipeline_at(5,{&invalid.reads,&invalid.reads,&invalid.reads,&invalid.reads,&invalid.reads,&invalid.reads,&invalid.reads,&invalid.reads,&invalid.reads,&invalid.reads},"capacity");
            if(kind==3) invalid.replay_pipeline_at(123,5,"missing capture");
            assert(false);
        } catch(std::runtime_error const &) { }
        assert(!invalid.m_pipeline_replay && invalid.m_phase==9 && invalid.events.empty());
    }
}
'''.replace('HELPERS', header[begin:end])
        if not shutil.which('c++'):
            self.skipTest('C++ compiler unavailable')
        with compiled_cpp(source) as program:
            subprocess.run([str(program)], check=True, capture_output=True)

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
