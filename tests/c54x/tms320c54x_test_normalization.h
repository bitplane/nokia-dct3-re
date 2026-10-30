// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_NORMALIZATION_H
#define MAME_NOKIA_TMS320C54X_TEST_NORMALIZATION_H

#include "tms320c54x_test_normalization_input.h"
#include "tms320c54x_test_normalization_schedule.h"
#include "tms320c54x_test_snapshot.h"
#include "tms320c54x_test_support.h"
#include "cpu/tms320c54x/tms320c54x.h"

namespace c54x_test {

class exponent_normalization_fixture
{
public:
	exponent_normalization_fixture(running_machine &machine, tms320c54x_device &cpu, emu_timer &timer, normalization_kind kind)
		: m_machine(machine), m_cpu(cpu), m_timer(timer), m_kind(kind) { }
	void start() { m_ordinal = m_restores = 0; m_coverage = {}; begin(); }
	unsigned count() const { return m_coverage.count; }
	unsigned restores() const { return m_restores; }
	u64 digest() const { return m_coverage.value; }
	bool step()
	{
		if (m_resume) { m_resume = false; m_cpu.resume(SUSPEND_REASON_DISABLE); schedule(); return false; }
		++m_tick;
		observe_normalization();
		if (m_tick == m_checkpoint && !m_replay)
		{
			m_saved_counts = m_counts;
			require(capture_snapshot(m_machine, m_saved) == STATERR_NONE, "capture retained operand/publication checkpoint");
		}
		if (m_tick == 20)
		{
			if (!m_replay)
			{
				m_replay = true; m_tick = m_checkpoint; m_counts = m_saved_counts;
				require(restore_snapshot(m_machine, m_timer, m_saved) == STATERR_NONE, "restore without resampling or duplicate accesses");
				++m_restores; m_cpu.suspend(SUSPEND_REASON_DISABLE, true); m_resume = true; m_timer.enable(true); return false;
			}
			m_read_tap.remove(); m_write_tap.remove(); m_fetch_tap.remove(); m_cpu.set_pipeline_frontend(false);
			if (++m_ordinal < normalization_schedule::count(m_kind)) { begin(); return false; }
			return true;
		}
		schedule(); return false;
	}
private:
	using r = tms320c54x_device;
	u64 state(int index) const { return m_cpu.state_int(index); }
	void require(bool condition, char const *message) const
	{
		if (!condition) throw emu_fatalerror("C54x normalization raw=%u tick=%u replay=%u: %s", m_raw, m_tick, unsigned(m_replay), message);
	}
	void schedule() { m_timer.adjust(attotime::from_ticks(1, m_cpu.clock())); }
	void observe_normalization()
	{
		auto const &f = m_norm;
		bool const retired = m_tick >= f.execute_cycle;
		require(!state(r::STATE_ILLEGAL), "NORM admits documented counts and recovered negative aliases");
		require(state(r::STATE_A) == (retired ? f.after[0] : f.before[0]) && state(r::STATE_B) == (retired ? f.after[1] : f.before[1]) && state(r::STATE_ST0) == (retired ? f.result_st0 : f.st0) && state(r::STATE_ST1) == f.st1, "NORM publishes its shifted result/sticky overflow only at Execute and preserves carry and other status");
		if (m_tick == f.read_cycle) m_cpu.set_state_int(r::STATE_T, 0xdead);
		if (m_tick == 20) require(m_counts[3] == 1 && state(r::STATE_IDLE) && state(r::STATE_T) == 0xdead, "NORM retains TS despite later T mutation and never rewrites T");
	}
	void begin()
	{
		m_cpu.suspend(SUSPEND_REASON_DISABLE, true);
		m_raw = normalization_schedule::key(m_kind, m_ordinal); m_coverage.add(m_raw);
		m_norm = normalization_case(m_raw, m_kind == normalization_kind::producer, m_kind == normalization_kind::aliases);
		auto const &n = m_norm;
		unsigned const prefix = n.prefix;
		u32 const base = n.base;
		auto &program = m_cpu.space(AS_PROGRAM); auto &data = m_cpu.space(AS_DATA);
		for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, 0xf495);
		if (prefix) program.write_word(base + 0x600, 0xf48e | ((n.selector >> 1) << 8));
		program.write_word(base + 0x600 + prefix + n.gap, 0xf48f | ((n.selector >> 1) << 9) | ((n.selector & 1) << 8));
		program.write_word(base + 0x601 + prefix + n.gap, 0xf4e1);
		m_checkpoint = m_kind == normalization_kind::aliases ? 3 + m_raw / 8192 : prefix ? std::array<unsigned, 4>{3, n.read_cycle - 1, n.read_cycle, n.execute_cycle + 1}[m_raw / 1536] : 3 + m_raw / 24576;
		m_cpu.set_state_int(r::STATE_IDLE, 0); m_cpu.set_state_int(r::STATE_ILLEGAL, 0); m_cpu.set_state_int(r::STATE_PMST, 0);
		m_cpu.set_state_int(r::STATE_ST0, n.st0); m_cpu.set_state_int(r::STATE_ST1, n.st1);
		m_cpu.set_state_int(r::STATE_T, n.t);
		m_cpu.set_state_int(r::STATE_A, n.before[0]);
		m_cpu.set_state_int(r::STATE_B, n.before[1]);
		m_cpu.set_state_int(r::STATE_TCR, 0x10); m_cpu.set_state_int(r::STATE_IFR, 0); m_cpu.set_state_int(r::STATE_IMR, 0);
		m_cpu.set_input_line(0, CLEAR_LINE); m_cpu.set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
		m_counts = {};
		m_read_tap = data.install_read_tap(0, 0xffff, "normalization_read", [this](offs_t, u16 &, u16) {
			if (m_machine.side_effects_disabled()) return;
			require(false, "NORM has no data-memory source");
		});
		m_write_tap = data.install_write_tap(0, 0xffff, "normalization_write", [this](offs_t, u16 &, u16) { if (!m_machine.side_effects_disabled()) { ++m_counts[2]; require(false, "NORM and EXP never write data memory"); } });
		u32 const opcode = base + 0x600 + prefix + n.gap;
		m_fetch_tap = program.install_read_tap(opcode, opcode, "normalization_fetch", [this](offs_t, u16 &, u16) { if (!m_machine.side_effects_disabled()) { require(!m_counts[3], "normalization opcode is fetched once"); ++m_counts[3]; } });
		m_replay = m_resume = false; m_tick = 0;
		m_cpu.set_state_int(STATE_GENPC, base + 0x600); m_cpu.set_pipeline_frontend(true); m_cpu.resume(SUSPEND_REASON_DISABLE); schedule();
	}
	running_machine &m_machine;
	tms320c54x_device &m_cpu;
	emu_timer &m_timer;
	normalization_kind m_kind;
	normalization_fixture m_norm{};
	unsigned m_raw = 0, m_ordinal = 0, m_tick = 0, m_checkpoint = 0, m_restores = 0;
	bool m_replay = false, m_resume = false;
	std::array<unsigned, 4> m_counts{}, m_saved_counts{};
	std::stringstream m_saved;
	ordered_case_digest m_coverage;
	memory_passthrough_handler m_read_tap, m_write_tap, m_fetch_tap;
};

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_NORMALIZATION_H
