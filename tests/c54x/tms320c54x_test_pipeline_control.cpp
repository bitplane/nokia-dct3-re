// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {










void tms320c54x_test_state::start_xc_return(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const zero = variant & 1, invert = variant & 2, wide = variant & 4;
	bool const selected = zero != invert;
	unsigned const source = (variant / 16) % 3; u32 const base = variant & 8 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) { program.write_word(base + 0x600 + i, 0xf495); program.write_word(base + 0x700 + i, 0xf495); }
	program.write_word(base + 0x600, invert ? 0xfd4c : 0xfd4d); program.write_word(base + 0x601, 0xfc00); program.write_word(base + 0x603, 0xf4e1);
	program.write_word(base + 0x700, wide ? 0x7660 : 0x8c60); if (wide) program.write_word(base + 0x701, 0xbeef);
	// The ordinary status probe is beyond the protected word's retirement;
	// adjacent status admission remains a separate conservative boundary.
	program.write_word(base + 0x706, 0x7707); program.write_word(base + 0x707, 0x0740); program.write_word(base + 0x708, 0xf4e1);
	program.write_word(base + 0x40, 0xf4e1); program.write_word(base + 4, 0xf4e1);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_ST0, 0xa020}, {cpu_regs::STATE_ST1, 0x100}, {cpu_regs::STATE_A, 0x123456789aULL},
		{cpu_regs::STATE_B, zero ? 0 : 1}, {cpu_regs::STATE_T, 0xbeef}, {cpu_regs::STATE_SP, 0x3000},
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 1},
	});
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	data.write_word(0x3000, 0x700); data.write_word(0x1060, 0xdead);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = m_vector_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "xc_return_stack", [this, selected](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(selected && at == 0x3000 && word == 0x700 && !m_pipeline_operand_reads++, "Selected XC return samples one stack target"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "xc_return_writes", [this, selected, source](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == 0x3000) { expect(selected && source && m_pipeline_store_writes == 1 && !m_pipeline_register_writes++, "Pending interrupt waits for the resumed protected word before stacking"); return; } if (at == 7) { expect(selected && word == 0x0740 && !m_pipeline_extensions_reads++, "Later ordinary STM ST1 is not trapped by stale XC protection"); return; } expect(selected && at == 0x1060 && word == 0xbeef && !m_pipeline_store_writes++, "Resumed protected instruction writes once"); });
	m_vector_prefetch_tap = program.install_read_tap(base + (source == 2 ? 4 : 0x40), base + (source == 2 ? 4 : 0x40), "xc_return_vector", [this, selected, source](offs_t, u16 &, u16) { if (machine().side_effects_disabled()) return; expect(selected && source && m_pipeline_store_writes == 1 && !m_vector_reads++, "XC protection expires after the resumed word and permits vector Fetch"); m_cpu->set_input_line(source == 2 ? INPUT_LINE_NMI : 0, CLEAR_LINE); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62176;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}















void tms320c54x_test_state::start_far_case(unsigned index)
{
	if (profile_is("far-atomic"))
	{
		set_registers(*m_cpu, {
			{cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_IMR, 0}, {cpu_regs::STATE_IFR, 0},
		});
	}
	auto &program = m_cpu->space(AS_PROGRAM);
	auto &data = m_cpu->space(AS_DATA);
	const u16 op = far_opcodes[index];
	const bool immediate = (op & 0xfc80) == 0xf880;
	const bool returning = (op & 0xfdfe) == 0xf4e4;
	const bool call = !returning && (immediate ? BIT(op, 8) : BIT(op, 0));
	unsigned next = 0x010600;
	program.write_word(next++, op);
	if (immediate)
		program.write_word(next++, 0x0700);
	if (BIT(op, 9))
	{
		program.write_word(next++, 0xe905);
		program.write_word(next++, 0xe906);
	}
	program.write_word(next, 0xf4e1);
	program.write_word(0x020700, 0xe807);
	program.write_word(0x020701, call ? 0xf4e4 : 0xf4e1);
	data.write_word(0x03fe, 2);
	data.write_word(0x03ff, 0x0700);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_SP, returning ? 0x03fe : 0x0400}, {cpu_regs::STATE_ST1, 0x0800}, {cpu_regs::STATE_A, 0x020700},
		{cpu_regs::STATE_B, 0x020700}, {STATE_GENPC, 0x010600}, {cpu_regs::STATE_IDLE, 0},
	});
	m_phase = 765 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}











bool tms320c54x_test_state::check_software_interrupt_entry_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 2701 && m_phase < 2765)
	{
		unsigned const index = m_phase - 2701;
		unsigned const vector = index & 31;
		u16 const base = (index & 1) ? 0x4000 : 0x2000;
		u16 const flags = vector >= 16 ? u16(0xffff & ~(1U << (vector - 16))) : 0xffff;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == base + vector * 4 + 3 &&
			m_cpu->state_int(tms320c54x_device::STATE_XPC) == 1 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0fff && data.read_word(0x0fff) == 0x09d3 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x0980 &&
			m_cpu->state_int(tms320c54x_device::STATE_IMR) == 0 &&
			m_cpu->state_int(tms320c54x_device::STATE_IFR) == flags &&
			m_port_writes == 2 && m_last_port_cycle - m_first_port_cycle == 5,
			"INTR selects IPTR:K, ignores masks, saves next PC, sets INTM, clears only selected IFR and costs three cycles");
		program.write_word(0x10000 | (base + vector * 4 + 3), 0xf49b); // RETF
		program.write_word(0x0109d3, 0xf4e1);
		data.write_word(0x0fff, 0xdead); // RTN, not this stack word, must supply PC.
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		m_phase = 61000 + index;
		m_check_timer->adjust(attotime::from_usec(100));
		return true;
	}
	return false;
}



















} // namespace c54x_test
