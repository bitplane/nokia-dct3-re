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
// Optional data events specify the complete ordered bus trace, including cycles.
class register_sequence_fixture
{
public:
	using builder = std::function<clocked_register_case(unsigned)>;
	register_sequence_fixture(running_machine &machine, tms320c54x_device &cpu, emu_timer &timer, unsigned cases, builder build)
		: m_machine(machine), m_cpu(cpu), m_timer(timer), m_cases(cases), m_build(std::move(build)) { }
	void start() { require(m_cases > 0, "nonempty register sequence"); m_case = m_restores = 0; m_coverage = {}; begin(); }
	unsigned count() const { return m_case; }
	unsigned restores() const { return m_restores; }
	void set_opcode_observer(std::function<void(u16, u16)> observer) { m_opcode_observer = std::move(observer); }
	u64 digest() const { return m_coverage.value; }
	bool step()
	{
		if (m_resume) { m_resume = false; m_cpu.resume(SUSPEND_REASON_DISABLE); schedule(); return false; }
		++m_tick;
		auto const *expected = &m_input.states.front().observation;
		for (auto const &state : m_input.states) if (m_tick >= state.tick) expected = &state.observation;
		check_case_state(m_cpu, *expected);
		// A clock's mutations follow observation but precede snapshot capture.
		for (auto const &action : m_input.actions) if (m_tick == action.tick)
			for (auto const &value : action.values) m_cpu.set_state_int(value.index, value.value);
		if (m_tick == m_input.checkpoint && !m_replay)
		{
			m_saved_fetches = m_fetches;
			m_saved_data_cursor = m_data_cursor;
			require(capture_snapshot(m_machine, m_saved) == STATERR_NONE, "capture register-sequence checkpoint");
		}
		if (m_tick == m_input.finish)
		{
			require(m_cpu.state_int(tms320c54x_device::STATE_IDLE), "sequence reaches its real IDLE instruction");
			if (m_input.single_fetch) require(m_fetches == 1, "one opcode fetch");
			if (m_input.data_events) require(m_data_cursor == m_input.data_events->size(), "complete data-bus trace");
			if (!m_replay)
			{
				m_replay = true; m_tick = m_input.checkpoint; m_fetches = m_saved_fetches;
				m_data_cursor = m_saved_data_cursor;
				require(restore_snapshot(m_machine, m_timer, m_saved) == STATERR_NONE, "restore register sequence");
				++m_restores; m_cpu.suspend(SUSPEND_REASON_DISABLE, true); m_resume = true; m_timer.enable(true); return false;
			}
			m_data_tap.remove(); m_fetch_tap.remove(); m_cpu.set_pipeline_frontend(false);
			m_coverage.add(m_input.identity.value_or(m_case));
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
		for (unsigned i = 0; i < m_input.actions.size(); ++i)
			require(m_input.actions[i].tick && m_input.actions[i].tick <= m_input.finish &&
				(!i || m_input.actions[i - 1].tick < m_input.actions[i].tick), "ordered action clocks before completion");
		require(m_input.states.back().tick <= m_input.finish, "all observations precede completion");
		require(m_input.checkpoint && m_input.checkpoint < m_input.finish, "checkpoint precedes completion");
		m_label = std::string(m_input.states.front().observation.id) + "/" + std::to_string(m_input.identity.value_or(m_case));
		for (auto &state : m_input.states) state.observation.id = m_label.c_str();
		prepare_case(m_cpu, m_input.states.front().observation);
		m_fetches = m_data_cursor = 0;
		require(!(m_input.forbid_data_access && m_input.data_events), "one data-bus observation policy");
		if (m_input.data_events)
		{
			auto access = [this](bool write, offs_t at, u16 word) {
				if (!m_machine.side_effects_disabled())
					require(accept_data_event(*m_input.data_events, m_data_cursor,
						{write, u32(at), word, unsigned(m_cpu.pipeline_cycles())}), "ordered data-bus event");
			};
			m_data_tap = m_cpu.space(AS_DATA).install_readwrite_tap(0, 0xffff, "register_sequence_data",
				[access](offs_t at, u16 &word, u16) { access(false, at, word); },
				[access](offs_t at, u16 &word, u16) { access(true, at, word); });
		}
		if (m_input.forbid_data_access)
		{
			auto access = [this](offs_t, u16 &, u16) { if (!m_machine.side_effects_disabled()) require(false, "register-only sequence has no data access"); };
			m_data_tap = m_cpu.space(AS_DATA).install_readwrite_tap(0, 0xffff, "register_sequence_data", access, access);
		}
		if (m_input.single_fetch)
			m_fetch_tap = m_cpu.space(AS_PROGRAM).install_read_tap(*m_input.single_fetch, *m_input.single_fetch, "register_sequence_opcode",
				[this](offs_t, u16 &word, u16) {
					if (m_machine.side_effects_disabled()) return;
					u16 const opcode = m_input.states.front().observation.opcode;
					require(!m_fetches && word == opcode, "one fetch of the specified opcode"); ++m_fetches;
					if (m_opcode_observer) m_opcode_observer(word, opcode);
				});
		m_tick = 0; m_replay = m_resume = false;
		m_cpu.set_pipeline_frontend(true); m_cpu.resume(SUSPEND_REASON_DISABLE); schedule();
	}
	running_machine &m_machine;
	tms320c54x_device &m_cpu;
	emu_timer &m_timer;
	unsigned m_cases, m_case = 0, m_tick = 0, m_restores = 0;
	unsigned m_fetches = 0, m_saved_fetches = 0;
	unsigned m_data_cursor = 0, m_saved_data_cursor = 0;
	builder m_build;
	std::function<void(u16, u16)> m_opcode_observer;
	clocked_register_case m_input;
	bool m_replay = false, m_resume = false;
	std::string m_label;
	std::stringstream m_saved;
	ordered_case_digest m_coverage;
	memory_passthrough_handler m_data_tap, m_fetch_tap;
};

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_REGISTER_SEQUENCE_H
