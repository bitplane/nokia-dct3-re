// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_REGISTER_SEQUENCE_H
#define MAME_NOKIA_TMS320C54X_TEST_REGISTER_SEQUENCE_H

#include "tms320c54x_test_case.h"
#include "tms320c54x_test_snapshot.h"
#include "cpu/tms320c54x/tms320c54x.h"
#include <algorithm>
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
	unsigned data_events_seen() const { return m_data_cursor; }
	void set_opcode_observer(std::function<void(u16, u16)> observer) { m_opcode_observer = std::move(observer); }
	void set_data_observer(std::function<void(bool, u32, u16)> observer) { m_data_observer = std::move(observer); }
	u64 digest() const { return m_coverage.value; }
	bool step()
	{
		if (m_resume) { m_resume = false; m_cpu.resume(SUSPEND_REASON_DISABLE); schedule(); return false; }
		++m_tick;
		if (m_input.check_pipeline_clock) require(m_cpu.pipeline_cycles() == m_tick, "pipeline advances one clock per observation");
		apply_actions(true);
		auto const *expected = &m_input.states.front().observation;
		for (auto const &state : m_input.states) if (m_tick >= state.tick) expected = &state.observation;
		{
			auto const disable = m_machine.disable_side_effects();
			check_case_state(m_cpu, *expected);
		}
		// A clock's mutations follow observation but precede snapshot capture.
		apply_actions(false);
		if (m_tick == m_input.checkpoint && !m_replay)
		{
			m_saved_fetches = m_fetches;
			m_saved_data_cursor = m_data_cursor;
			m_saved_program_cursor = m_program_cursor;
			require(capture_snapshot(m_machine, m_saved) == STATERR_NONE, "capture register-sequence checkpoint");
		}
		if (m_tick == m_input.finish)
		{
			if (m_input.final_pipeline_clock) require(m_cpu.pipeline_cycles() == *m_input.final_pipeline_clock, "final pipeline clock matches the declared stop");
			if (m_input.require_idle) require(m_cpu.state_int(tms320c54x_device::STATE_IDLE), "sequence reaches its real IDLE instruction");
			if (m_input.single_fetch) require(m_fetches == 1, "one opcode fetch");
			if (m_input.data_events) require(m_data_cursor == m_input.data_events->size(), "complete data-bus trace");
			require(m_program_cursor == (m_input.program_events.empty() ? m_input.retained_program_reads.size() : m_input.program_events.size()), "complete retained-program trace");
			if (!m_replay)
			{
				m_replay = true; m_tick = m_input.checkpoint; m_fetches = m_saved_fetches;
				m_data_cursor = m_saved_data_cursor;
				m_program_cursor = m_saved_program_cursor;
				if (m_input.clear_pipeline_before_restore) m_cpu.set_pipeline_frontend(true);
				require(restore_snapshot(m_machine, m_timer, m_saved) == STATERR_NONE, "restore register sequence");
				++m_restores; m_cpu.suspend(SUSPEND_REASON_DISABLE, true); m_resume = true; m_timer.enable(true); return false;
			}
			m_data_tap.remove(); m_fetch_tap.remove(); m_program_tap.remove(); m_cpu.set_pipeline_frontend(false);
			m_coverage.add(m_input.identity.value_or(m_case));
			if (++m_case < m_cases) { begin(); return false; }
			return true;
		}
		schedule(); return false;
	}
private:
	void apply_actions(bool before)
	{
		for (auto const &action : m_input.actions) if (m_tick == action.tick && action.before_observation == before)
		{
			for (auto const &value : action.values) m_cpu.set_state_int(value.index, value.value);
			for (auto const &line : action.input_lines) m_cpu.set_input_line(line.index, int(line.value));
			if (!action.program.empty() || !action.data.empty())
			{
				auto const disable = m_machine.disable_side_effects();
				for (auto const &word : action.program) m_cpu.space(AS_PROGRAM).write_word(word.address, word.value);
				for (auto const &word : action.data) m_cpu.space(AS_DATA).write_word(word.address, word.value);
			}
		}
	}
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
		m_fetches = m_data_cursor = m_program_cursor = 0;
		require(!(m_input.forbid_data_access && m_input.data_events), "one data-bus observation policy");
		if (m_input.data_events)
		{
			auto access = [this](bool write, offs_t at, u16 word) {
				if (!m_machine.side_effects_disabled() && !accept_data_event(*m_input.data_events, m_data_cursor,
					{write, u32(at), word, unsigned(m_cpu.pipeline_cycles())}))
					throw emu_fatalerror("C54x case=%u event=%u write=%u address=%x value=%x clock=%llu",
						m_case, m_data_cursor, unsigned(write), unsigned(at), word, (unsigned long long)m_cpu.pipeline_cycles());
				if (!m_machine.side_effects_disabled() && m_data_observer) m_data_observer(write, u32(at), word);
			};
			auto const range = m_input.data_range.value_or(std::array<u32,2>{0,0xffff});
			m_data_tap = m_cpu.space(AS_DATA).install_readwrite_tap(range[0], range[1], "register_sequence_data",
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
		if (!m_input.program_events.empty())
		{
			require(m_input.retained_program_reads.empty(), "one program-read policy");
			u32 first = ~u32(0), last = 0;
			for (auto const &event : m_input.program_events) { first = std::min(first,event.address); last = std::max(last,event.address); }
			m_program_tap = m_cpu.space(AS_PROGRAM).install_read_tap(first,last,"register_sequence_program",
				[this](offs_t at, u16 &word, u16) {
					if (m_machine.side_effects_disabled()) return;
					bool selected = false;
					for (auto const &event : m_input.program_events) selected |= event.address == at;
					if (selected && !accept_data_event(m_input.program_events,m_program_cursor,
						{false,u32(at),word,unsigned(m_cpu.pipeline_cycles())}))
						throw emu_fatalerror("C54x program case=%u event=%u address=%x value=%x clock=%llu",m_case,m_program_cursor,
							unsigned(at),word,(unsigned long long)m_cpu.pipeline_cycles());
				});
		}
		else if (!m_input.retained_program_reads.empty())
		{
			auto const &words = m_input.retained_program_reads;
			for (unsigned i = 1; i < words.size(); ++i)
				require(words[i].address == words[i - 1].address + 1, "contiguous retained-program words");
			m_program_tap = m_cpu.space(AS_PROGRAM).install_read_tap(words.front().address, words.back().address,
				"register_sequence_literals", [this](offs_t at, u16 &word, u16) {
					if (m_machine.side_effects_disabled()) return;
					auto const &words = m_input.retained_program_reads;
					require(m_program_cursor < words.size() && at == words[m_program_cursor].address &&
						word == words[m_program_cursor].value, "ordered retained-program word");
					++m_program_cursor;
				});
		}
		m_tick = 0; m_replay = m_resume = false;
		m_cpu.set_pipeline_frontend(true); m_cpu.resume(SUSPEND_REASON_DISABLE); schedule();
	}
	running_machine &m_machine;
	tms320c54x_device &m_cpu;
	emu_timer &m_timer;
	unsigned m_cases, m_case = 0, m_tick = 0, m_restores = 0;
	unsigned m_fetches = 0, m_saved_fetches = 0;
	unsigned m_data_cursor = 0, m_saved_data_cursor = 0;
	unsigned m_program_cursor = 0, m_saved_program_cursor = 0;
	builder m_build;
	std::function<void(u16, u16)> m_opcode_observer;
	std::function<void(bool, u32, u16)> m_data_observer;
	clocked_register_case m_input;
	bool m_replay = false, m_resume = false;
	std::string m_label;
	std::stringstream m_saved;
	ordered_case_digest m_coverage;
	memory_passthrough_handler m_data_tap, m_fetch_tap, m_program_tap;
};

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_REGISTER_SEQUENCE_H
