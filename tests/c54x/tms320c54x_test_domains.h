// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_DOMAINS_H
#define MAME_NOKIA_TMS320C54X_TEST_DOMAINS_H

#include "tms320c54x_test_case.h"
#include "tms320c54x_test_domain_schedule.h"
#include "tms320c54x_test_snapshot.h"
#include "cpu/tms320c54x/tms320c54x.h"
#include <string>

namespace c54x_test {

// Admission-limit checks, not a claim that every rejected form is illegal on
// silicon. This fixture owns no handset devices or global phase identifiers.
class source_domain_fixture
{
public:
	source_domain_fixture(running_machine &machine, tms320c54x_device &cpu, emu_timer &timer)
		: m_machine(machine), m_cpu(cpu), m_timer(timer) { }
	void start() { m_variant = m_restores = 0; m_coverage = {}; begin(); }
	unsigned count() const { return m_coverage.count; }
	unsigned restores() const { return m_restores; }
	u64 digest() const { return m_coverage.value; }
	bool step()
	{
		if (m_resume)
		{
			m_resume = false; m_cpu.resume(SUSPEND_REASON_DISABLE); schedule(); return false;
		}
		++m_tick;
		require(!m_reads && !m_writes && !m_program_writes && !m_io,
			"rejection precedes data/program/I-O effects");
		check_case_state(m_cpu, m_case);
		unsigned const checkpoint = m_input.checkpoint ? m_input.checkpoint : (context() & 8 ? 10 : 1);
		if (m_tick == checkpoint && !m_replay)
			require(capture_snapshot(m_machine, m_saved) == STATERR_NONE, "capture rejection checkpoint");
		if (m_tick != 20) { schedule(); return false; }
		require(m_cpu.state_int(tms320c54x_device::STATE_ILLEGAL), "rejected form reaches illegal latch");
		if (!m_replay)
		{
			m_replay = true; m_tick = checkpoint;
			require(restore_snapshot(m_machine, m_timer, m_saved) == STATERR_NONE, "restore rejection checkpoint");
			++m_restores;
			m_cpu.suspend(SUSPEND_REASON_DISABLE, true); m_resume = true; m_timer.enable(true);
			return false;
		}
		clear_taps(); m_cpu.set_pipeline_frontend(false);
		m_coverage.add(m_input.key());
		if (++m_variant < domain_total_cases) { begin(); return false; }
		require(count() == domain_total_cases && m_restores == domain_total_cases, "complete case and replay counts");
		return true;
	}
private:
	unsigned context() const { return m_input.context; }
	void require(bool condition, const char *message) const
	{
		if (!condition) throw emu_fatalerror("C54x domain case=%u tick=%u replay=%u: %s",
			m_variant, m_tick, unsigned(m_replay), message);
	}
	void schedule() { m_timer.adjust(attotime::from_ticks(1, m_cpu.clock())); }
	void clear_taps() { m_read_tap.remove(); m_write_tap.remove(); m_program_tap.remove(); m_io_tap.remove(); }
	void begin()
	{
		using r = tms320c54x_device;
		clear_taps(); m_cpu.reset(); m_cpu.suspend(SUSPEND_REASON_DISABLE, true);
		m_tick = 0; m_replay = m_resume = false;
		m_input = select_domain(m_variant);
		unsigned const mode = m_input.mode;
		auto const instruction = m_input.instruction;
		u32 const base = context() & 2 ? 0x20000 : 0;
		m_id = "domain/" + std::to_string(m_variant);
		m_case = {m_id.c_str(), u16(instruction.opcode | mode),
			{{base + 0x600, instruction.opcode | mode},
			 {base + 0x601, instruction.operation && mode < 0xe0 ? instruction.operation : 0x1500},
			 {base + 0x602, instruction.operation && mode >= 0xe0 ? instruction.operation : 3},
			 {base + 0x603, 0xf4e1}}, {}, {},
			{{r::STATE_ST0, u64(m_input.st0)}, {r::STATE_ST1, u64(m_input.st1 | (context() & 1 ? 0x20 : 0))},
			 {r::STATE_A, 0x123456789aULL}, {r::STATE_B, 0xfedcba9876ULL},
			 {r::STATE_T, 0xabcd}, {r::STATE_SP, 0x1600}}, 0, 0};
		if (m_input.absolute_source != 0xffff)
		{
			m_case.program[1].value = m_input.absolute_source;
			m_case.program[2].value = 9;
		}
		else if (m_input.checkpoint)
			m_case.program[2].value = 0xf4e1; // Retained two-word MAC guard program.
		if (m_input.prefix)
			m_case.program = {{base + 0x600, m_input.prefix}, {base + 0x601, m_case.opcode}, {base + 0x602, 0xf4e1}};
		else
			m_case.expected.push_back({r::STATE_RC, 7});
		// Rejection preserves the supplied values, not results read from the CPU.
		for (auto const &value : m_case.expected) m_case.initial.emplace_back(value.index, value.value);
		m_case.initial.insert(m_case.initial.end(), {{r::STATE_IMR, 0}, {r::STATE_IFR, 0}, {r::STATE_TCR, 0x10}});
		for (unsigned i = 0; i < 8; ++i)
		{
			m_case.initial.emplace_back(r::STATE_AR0 + i, 0x1400 + i);
			m_case.expected.push_back({int(r::STATE_AR0) + int(i), 0x1400 + i});
		}
		prepare_case(m_cpu, m_case);
		m_reads = m_writes = m_program_writes = m_io = 0;
		m_read_tap = m_cpu.space(AS_DATA).install_read_tap(0, 0xffff, "domain_data_read",
			[this](offs_t, u16 &, u16) { if (!m_machine.side_effects_disabled()) ++m_reads; });
		m_write_tap = m_cpu.space(AS_DATA).install_write_tap(0, 0xffff, "domain_data_write",
			[this](offs_t, u16 &, u16) { if (!m_machine.side_effects_disabled()) ++m_writes; });
		m_program_tap = m_cpu.space(AS_PROGRAM).install_write_tap(0, 0x7fffff, "domain_program_write",
			[this](offs_t, u16 &, u16) { if (!m_machine.side_effects_disabled()) ++m_program_writes; });
		m_io_tap = m_cpu.space(AS_IO).install_readwrite_tap(0, 0xffff, "domain_io_access",
			[this](offs_t, u16 &, u16) { if (!m_machine.side_effects_disabled()) ++m_io; },
			[this](offs_t, u16 &, u16) { if (!m_machine.side_effects_disabled()) ++m_io; });
		m_cpu.set_state_int(STATE_GENPC, base + 0x600);
		m_cpu.set_pipeline_frontend(!(context() & 4)); m_cpu.resume(SUSPEND_REASON_DISABLE); schedule();
	}
	running_machine &m_machine;
	tms320c54x_device &m_cpu;
	emu_timer &m_timer;
	// Host observations remain outside guest save states, as in the original fixture.
	unsigned m_variant = 0, m_tick = 0, m_restores = 0;
	unsigned m_reads = 0, m_writes = 0, m_program_writes = 0, m_io = 0;
	bool m_replay = false, m_resume = false;
	std::string m_id;
	domain_input m_input{};
	instruction_case m_case{};
	std::stringstream m_saved;
	ordered_case_digest m_coverage;
	memory_passthrough_handler m_read_tap, m_write_tap, m_program_tap, m_io_tap;
};

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_DOMAINS_H
