// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_DOMAINS_H
#define MAME_NOKIA_TMS320C54X_TEST_DOMAINS_H

#include "tms320c54x_test_case.h"
#include "tms320c54x_test_snapshot.h"
#include "cpu/tms320c54x/tms320c54x.h"
#include <string>

namespace c54x_test {

struct source_domain_instruction
{
	u16 opcode;
	u16 operation;
};

// Independent instruction inventory: SPRU172C's single-data-memory read/RMW
// forms, shifted read extensions, and the four implemented MMR-source forms.
// Do not derive this bank from the CPU's admission predicates.
constexpr source_domain_instruction source_domains[] = {
	{ 0x0000, 0 }, { 0x0100, 0 }, { 0x0200, 0 }, { 0x0300, 0 },
	{ 0x0400, 0 }, { 0x0500, 0 }, { 0x0600, 0 }, { 0x0700, 0 },
	{ 0x0800, 0 }, { 0x0900, 0 }, { 0x0a00, 0 }, { 0x0b00, 0 },
	{ 0x0c00, 0 }, { 0x0d00, 0 }, { 0x0e00, 0 }, { 0x0f00, 0 },
	{ 0x1000, 0 }, { 0x1100, 0 }, { 0x1200, 0 }, { 0x1300, 0 },
	{ 0x1400, 0 }, { 0x1500, 0 }, { 0x1600, 0 }, { 0x1700, 0 },
	{ 0x1800, 0 }, { 0x1900, 0 }, { 0x1a00, 0 }, { 0x1b00, 0 },
	{ 0x1c00, 0 }, { 0x1d00, 0 }, { 0x1e00, 0 }, { 0x1f00, 0 },
	{ 0x2000, 0 }, { 0x2100, 0 }, { 0x2200, 0 }, { 0x2300, 0 },
	{ 0x2400, 0 }, { 0x2500, 0 }, { 0x2600, 0 }, { 0x2700, 0 },
	{ 0x2800, 0 }, { 0x2900, 0 }, { 0x2a00, 0 }, { 0x2b00, 0 },
	{ 0x2c00, 0 }, { 0x2d00, 0 }, { 0x2e00, 0 }, { 0x2f00, 0 },
	{ 0x3000, 0 }, { 0x3100, 0 }, { 0x3200, 0 }, { 0x3300, 0 },
	{ 0x3400, 0 }, { 0x3500, 0 }, { 0x3600, 0 }, { 0x3700, 0 },
	{ 0x3800, 0 }, { 0x3900, 0 }, { 0x3a00, 0 }, { 0x3b00, 0 },
	{ 0x3c00, 0 }, { 0x3d00, 0 }, { 0x3e00, 0 }, { 0x3f00, 0 },
	{ 0x4000, 0 }, { 0x4100, 0 }, { 0x4200, 0 }, { 0x4300, 0 },
	{ 0x4400, 0 }, { 0x4500, 0 }, { 0x4600, 0 }, { 0x4700, 0 },
	{ 0x4b00, 0 }, { 0x4c00, 0 }, { 0x4d00, 0 }, { 0x5000, 0 },
	{ 0x5100, 0 }, { 0x5200, 0 }, { 0x5300, 0 }, { 0x5400, 0 },
	{ 0x5500, 0 }, { 0x5600, 0 }, { 0x5700, 0 }, { 0x5800, 0 },
	{ 0x5900, 0 }, { 0x5a00, 0 }, { 0x5b00, 0 }, { 0x5c00, 0 },
	{ 0x5d00, 0 }, { 0x5e00, 0 }, { 0x5f00, 0 }, { 0x6000, 0 },
	{ 0x6100, 0 }, { 0x6200, 0 }, { 0x6300, 0 }, { 0x6400, 0 },
	{ 0x6500, 0 }, { 0x6600, 0 }, { 0x6700, 0 }, { 0x6800, 0 },
	{ 0x6900, 0 }, { 0x6a00, 0 }, { 0x6b00, 0 }, { 0x7100, 0 },
	{ 0x7500, 0 }, { 0x7800, 0 }, { 0x7900, 0 }, { 0x7a00, 0 },
	{ 0x7b00, 0 }, { 0x7d00, 0 }, { 0x7f00, 0 }, { 0x6f00, 0x0c40 },
	{ 0x6f00, 0x0d40 }, { 0x6f00, 0x0c00 }, { 0x6f00, 0x0d00 }, { 0x6f00, 0x0e00 },
	{ 0x6f00, 0x0f00 }, { 0x6f00, 0x0c20 }, { 0x6f00, 0x0d20 }, { 0x6f00, 0x0e20 },
	{ 0x6f00, 0x0f20 }, { 0x4800, 0 }, { 0x4900, 0 }, { 0x4a00, 0 },
	{ 0x7300, 0 },
};
static_assert(std::size(source_domains) == 125);
constexpr unsigned domain_cases = 125 * 15 * 16;

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
		unsigned const checkpoint = context() & 8 ? 10 : 1;
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
		auto const instruction = source_domains[m_variant / (15 * 16)];
		m_coverage.add((u32(m_case.opcode) << 16) | (u32(instruction.operation) << 4) | context());
		if (++m_variant < domain_cases) { begin(); return false; }
		require(count() == domain_cases && m_restores == domain_cases, "complete case and replay counts");
		return true;
	}
private:
	unsigned context() const { return (m_variant / 15) % 16; }
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
		unsigned const mode_index = m_variant % 15;
		u16 const mode = mode_index < 8 ? 0x98 + mode_index : 0xf9 + mode_index - 8;
		auto const instruction = source_domains[m_variant / (15 * 16)];
		u32 const base = context() & 2 ? 0x20000 : 0;
		m_id = "domain/" + std::to_string(m_variant);
		m_case = {m_id.c_str(), u16(instruction.opcode | mode),
			{{base + 0x600, instruction.opcode | mode},
			 {base + 0x601, instruction.operation && mode < 0xe0 ? instruction.operation : 0x1500},
			 {base + 0x602, instruction.operation && mode >= 0xe0 ? instruction.operation : 3},
			 {base + 0x603, 0xf4e1}}, {}, {},
			{{r::STATE_ST0, 0x1804}, {r::STATE_ST1, u64(0x800 | (context() & 1 ? 0x20 : 0))},
			 {r::STATE_A, 0x123456789aULL}, {r::STATE_B, 0xfedcba9876ULL},
			 {r::STATE_T, 0xabcd}, {r::STATE_RC, 7}, {r::STATE_SP, 0x1600}}, 0, 0};
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
	instruction_case m_case{};
	std::stringstream m_saved;
	ordered_case_digest m_coverage;
	memory_passthrough_handler m_read_tap, m_write_tap, m_program_tap, m_io_tap;
};

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_DOMAINS_H
