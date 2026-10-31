"""Exercise retained firmware replay scheduling; not generic CPU semantics."""
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class FirmwareReplayTests(unittest.TestCase):
    def test_timed_restore_cancels_loads_then_suspends_before_continuation(self):
        from tools.test_c54x_case_fixture import compiled_cpp
        header = (ROOT / 'tests/firmware/tms320c54x_test_state.h').read_text()
        begin = header.index('\tbool restore_pipeline_at(')
        end = header.index('\tbool check_rom4_fr_decoder_results(', begin)
        restart = header[header.index('\tbool restart_pipeline_observer('):begin]
        source = r'''
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
using u64 = std::uint64_t;
struct attotime {
    static constexpr int never = -1;
    static int from_ticks(int ticks, int clock) { assert(ticks == 1 && clock == 13); return 23; }
};
constexpr int STATERR_NONE = 0, SUSPEND_REASON_DISABLE = 1;
#include "tests/c54x/tms320c54x_test_snapshot.h"
using c54x_test::restore_snapshot;
struct bench {
    std::vector<int> events;
    unsigned m_phase = 9;
    u64 m_pipeline_expected_cycle = 8;
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
        int clock() const { return 13; }
        void suspend(int reason, bool eat_clocks) {
            assert(reason == SUSPEND_REASON_DISABLE && eat_clocks && owner.m_phase == 9);
            assert(owner.m_pipeline_expected_cycle == 8);
            owner.events.push_back(4);
        }
    } core{*this}, *m_cpu = &core;
    struct timer {
        bench &owner;
        void adjust(int when) {
            if (when == attotime::never) { owner.events.push_back(1); return; }
            assert(when == 23 && owner.m_phase == 123 && owner.m_pipeline_expected_cycle == 44);
            owner.events.push_back(6);
        }
        void enable(bool enabled) {
            assert(enabled && owner.m_phase == 123); owner.events.push_back(5);
        }
    } clock{*this}, *m_check_timer = &clock;
''' + header[begin:end] + restart + r'''
};
int main() {
    bench ok; ok.m_saved_pipeline.seekg(5); ok.m_saved_pipeline.setstate(std::ios::eofbit);
    assert(ok.restore_pipeline_at(123, "retained state"));
    assert((ok.events == std::vector<int>{1,2,3,4,5}));
    bench bad; bad.fail_load = true;
    try { bad.restore_pipeline_at(123, "retained state"); assert(false); }
    catch (std::runtime_error const &) { }
    assert(bad.m_phase == 9 && (bad.events == std::vector<int>{1,2,3}));
    bench explicit_tick;
    assert(explicit_tick.restart_pipeline_observer(123, 44));
    assert((explicit_tick.events == std::vector<int>{4,6}));
}
'''
        if not shutil.which('c++'):
            self.skipTest('C++ compiler unavailable')
        with compiled_cpp(source) as program:
            subprocess.run([str(program)], check=True, capture_output=True)


if __name__ == '__main__':
    unittest.main()
