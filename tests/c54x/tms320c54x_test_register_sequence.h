// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_REGISTER_SEQUENCE_H
#define MAME_NOKIA_TMS320C54X_TEST_REGISTER_SEQUENCE_H

#include "tms320c54x_test_case.h"
#include "tms320c54x_test_snapshot.h"
#include "cpu/tms320c54x/tms320c54x.h"
#include <functional>

namespace c54x_test {

// Explicit expected states at CPU clocks, with no implied arithmetic or reset.
// The first state's case also owns setup; later states only own observations.
class register_sequence_fixture
{
public:
	using builder = std::function<clocked_register_case(unsigned)>;
	register_sequence_fixture(running_machine &machine, tms320c54x_device &cpu, emu_timer &timer, unsigned cases, builder build)
		: m_machine(machine), m_cpu(cpu), m_timer(timer), m_cases(cases), m_build(std::move(build)) { }
	void start() { require(m_cases > 0, "nonempty register sequence"); m_case = m_restores = 0; m_coverage = {}; begin(); }
	unsigned count() const { return m_case; }
	unsigned restores() const { return m_restores; }
	u64 digest() const { return m_coverage.value; }
	bool step()
	{
		if (m_resume) { m_resume = false; m_cpu.resume(SUSPEND_REASON_DISABLE); schedule(); return false; }
		++m_tick;
		auto const *expected = &m_input.states.front().observation;
		for (auto const &state : m_input.states) if (m_tick >= state.tick) expected = &state.observation;
		check_case_state(m_cpu, *expected);
		if (m_tick == m_input.checkpoint && !m_replay)
			require(capture_snapshot(m_machine, m_saved) == STATERR_NONE, "capture register-sequence checkpoint");
		if (m_tick == m_input.finish)
		{
			require(m_cpu.state_int(tms320c54x_device::STATE_IDLE), "sequence reaches its real IDLE instruction");
			if (!m_replay)
			{
				m_replay = true; m_tick = m_input.checkpoint;
				require(restore_snapshot(m_machine, m_timer, m_saved) == STATERR_NONE, "restore register sequence");
				++m_restores; m_cpu.suspend(SUSPEND_REASON_DISABLE, true); m_resume = true; m_timer.enable(true); return false;
			}
			m_cpu.set_pipeline_frontend(false); m_coverage.add(m_case);
			if (++m_case < m_cases) { begin(); return false; }
			return true;
		}
		schedule(); return false;
	}
private:
	void require(bool value, char const *message) const
	{
		if (!value) throw emu_fatalerror("C54x register sequence case=%u tick=%u replay=%u: %s", m_case, m_tick, unsigned(m_replay), message);
	}
	void schedule() { m_timer.adjust(attotime::from_ticks(1, m_cpu.clock())); }
	void begin()
	{
		m_cpu.suspend(SUSPEND_REASON_DISABLE, true); m_input = m_build(m_case);
		require(!m_input.states.empty() && m_input.states.front().tick == 0, "explicit initial observation");
		for (unsigned i = 1; i < m_input.states.size(); ++i)
			require(m_input.states[i - 1].tick < m_input.states[i].tick, "ordered observation clocks");
		require(m_input.states.back().tick <= m_input.finish, "all observations precede completion");
		require(m_input.checkpoint && m_input.checkpoint < m_input.finish, "checkpoint precedes completion");
		m_label = std::string(m_input.states.front().observation.id) + "/" + std::to_string(m_case);
		for (auto &state : m_input.states) state.observation.id = m_label.c_str();
		prepare_case(m_cpu, m_input.states.front().observation);
		m_tick = 0; m_replay = m_resume = false;
		m_cpu.set_pipeline_frontend(true); m_cpu.resume(SUSPEND_REASON_DISABLE); schedule();
	}
	running_machine &m_machine;
	tms320c54x_device &m_cpu;
	emu_timer &m_timer;
	unsigned m_cases, m_case = 0, m_tick = 0, m_restores = 0;
	builder m_build;
	clocked_register_case m_input;
	bool m_replay = false, m_resume = false;
	std::string m_label;
	std::stringstream m_saved;
	ordered_case_digest m_coverage;
};

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_REGISTER_SEQUENCE_H
