// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

void tms320c54x_test_state::start_instruction_observer(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_instruction_observer.variant = variant;
	bool const xc = variant >= 32, two = xc && BIT(variant - 32, 5);
	bool const staged = BIT(variant, 0), overlay = BIT(variant, 2);
	u32 const base = BIT(variant, 1) ? 0x20000 : 0, physical = overlay ? 0 : base;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 48; ++i) program.write_word(physical + 0x600 + i, 0xf495);
	std::array<u16, 10> const code{0xec02, 0xf495, 0xf073, 0x0608, 0xe966, 0xf495, 0xf495, 0xf495, 0xe977, 0xf4e1};
	for (unsigned i = 0; i < code.size(); ++i) program.write_word(physical + 0x600 + i, code[i]);
	if (xc)
	{
		std::array<u16, 7> const conditional{u16(two ? 0xff45 : 0xfd45), 0xe966, u16(two ? 0xe955 : 0xf495),
			u16(two ? 0xff44 : 0xfd44), 0xe977, u16(two ? 0xf495 : 0xf4e1), 0xf4e1};
		for (unsigned i = 0; i < conditional.size(); ++i) program.write_word(physical + 0x600 + i, conditional[i]);
	}
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, overlay ? 0x20 : 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_ST0, 0}, {cpu_regs::STATE_ST1, 0x900}, {cpu_regs::STATE_SP, 0x1000},
		{cpu_regs::STATE_A, xc ? 1 : 0}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
	});
	m_instruction_observer.observed.clear(); m_instruction_observer.snapshot.clear();
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(staged);
	m_instruction_observer.replay = false; m_instruction_observer.tick = 0; m_phase = 292000;
	m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

bool tms320c54x_test_state::check_instruction_observer_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 292000)
	{
		unsigned const variant = m_instruction_observer.variant, tick = ++m_instruction_observer.tick;
		bool const xc = variant >= 32, two = xc && BIT(variant - 32, 5);
		u32 const base = BIT(variant, 1) ? 0x20000 : 0, physical = BIT(variant, 2) ? 0 : base;
		unsigned const checkpoint = std::array<unsigned, 4>{1, 3, 6, 9}[(variant % 32) / 8];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Observer fixture executes only accepted instructions");
		// Debugger-style inspection and plain program-bus reads are not execution.
		unsigned const before = m_instruction_observer.observed.size();
		m_cpu->space(AS_PROGRAM).read_word(physical + 0x600);
		{ auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).read_word(physical + 0x601); }
		expect(m_instruction_observer.observed.size() == before, "Program/data inspection cannot issue instruction observations");
		if (tick == 5)
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_PROGRAM).write_word(physical + 0x600, 0x0000);
		}
		if (tick == checkpoint && !m_instruction_observer.replay)
		{
			m_instruction_observer.snapshot = m_instruction_observer.observed;
			m_instruction_observer.saved.str({}); m_instruction_observer.saved.clear();
			expect(machine().save().write_stream(m_instruction_observer.saved) == STATERR_NONE, "Save before or after observer entry without saving callback wiring");
		}
		if (tick == 40)
		{
			std::vector<std::pair<offs_t, u16>> expected{
				{base + 0x600, 0xec02}, {base + 0x601, 0xf495}, {base + 0x601, 0xf495},
				{base + 0x601, 0xf495}, {base + 0x602, 0xf073}, {base + 0x608, 0xe977}, {base + 0x609, 0xf4e1}};
			if (xc)
			{
				expected = {{base + 0x600, u16(two ? 0xff45 : 0xfd45)}};
				if (!two) expected.emplace_back(base + 0x602, 0xf495);
				expected.emplace_back(base + 0x603, two ? 0xff44 : 0xfd44);
				expected.emplace_back(base + 0x604, 0xe977);
				if (two) expected.emplace_back(base + 0x605, 0xf495);
				expected.emplace_back(base + (two ? 0x606 : 0x605), 0xf4e1);
			}
			if (m_instruction_observer.observed != expected)
			{
				osd_printf_info("Instruction observer mismatch: case=%u replay=%u entries=%zu pc=%06x\n", variant, unsigned(m_instruction_observer.replay), m_instruction_observer.observed.size(), unsigned(m_cpu->state_int(STATE_GENPC)));
				for (auto const &[address, opcode] : m_instruction_observer.observed) osd_printf_info("  entry=%06x opcode=%04x\n", unsigned(address), opcode);
			}
			expect(m_instruction_observer.observed == expected && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x77 && m_cpu->state_int(tms320c54x_device::STATE_A) == (xc ? 1 : 0) && m_cpu->state_int(tms320c54x_device::STATE_IDLE),
				"Execute entries retain opcode and logical page, include real repeats and exclude extension/annulled fetches");
			if (!m_instruction_observer.replay)
			{
				m_instruction_observer.replay = true; m_instruction_observer.tick = checkpoint;
				m_instruction_observer.observed = m_instruction_observer.snapshot;
				m_instruction_observer.saved.clear(); m_instruction_observer.saved.seekg(0); m_check_timer->adjust(attotime::never);
				expect(machine().save().read_stream(m_instruction_observer.saved) == STATERR_NONE, "Restore instruction identity without extra observer emissions");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 292010; m_check_timer->enable(true); return true;
			}
			m_cpu->set_pipeline_frontend(false);
			if (variant == 31) osd_printf_info("TMS320C54x instruction observer: PASS cases=32 restore=32 frontends=atomic,staged pages=2 ovly_states=2 snapshots=4 events_each=7 logical_address=1 captured_opcode=1 repeat_entries=3 ordinary_reads=0 annulled_fetches=0 observer_writes=0\n");
			if (variant < 95) { start_instruction_observer(variant + 1); return true; }
			osd_printf_info("TMS320C54x observer XC annulment: PASS cases=64 restore=64 frontends=atomic,staged pages=2 ovly_states=2 snapshots=4 forms=XC1,XC2 events_each=5 rejected_slots=absent accepted_source_nops=present observer_writes=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated instruction observer complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 292010) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 292000; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

} // namespace c54x_test
