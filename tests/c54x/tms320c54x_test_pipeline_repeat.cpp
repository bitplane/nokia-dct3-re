// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

tms320c54x_test_state::delay_multiply_fixture tms320c54x_test_state::delay_square_case(unsigned variant)
{
	auto f = delay_accumulator_multiply_case(variant);
	f.square = true; f.destination = f.operation & 1; f.operation >>= 1;
	if (f.kind != 1 && f.kind != 2) f.a = f.b ^ 0x55;
	s64 const result = f.arithmetic();
	f.result = u64(f.flags & 2 ? std::clamp(result, s64(-2147483648LL), s64(2147483647LL)) : result) & 0xffffffffffULL;
	return f;
}

void tms320c54x_test_state::set_repeat_count_source(u16 address, u16 value)
{
	if (address >= 0x60) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(address, value); }
	else if (address < 14)
	{
		unsigned const part = (address - 8) % 3; auto const state = address < 11 ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B;
		u64 const mask = part == 2 ? 0xff : 0xffff;
		m_cpu->set_state_int(state, (m_cpu->state_int(state) & ~(mask << (16 * part))) | ((u64(value) & mask) << (16 * part)));
	}
	else m_cpu->set_state_int(address == 14 ? tms320c54x_device::STATE_T : address == 15 ? tms320c54x_device::STATE_TRN : address < 24 ? tms320c54x_device::STATE_AR0 + address - 16 : address == 24 ? tms320c54x_device::STATE_SP : tms320c54x_device::STATE_BK, value);
}


u16 tms320c54x_test_state::block_bcd_source(unsigned variant, unsigned iteration)
{
	unsigned const pattern = (variant / 48) % 4;
	return pattern == 0 ? 0 : pattern == 1 ? 1 : pattern == 2 ? 0xffff : iteration & 1 ? 0 : 0x8000;
}

bool tms320c54x_test_state::block_bcd_taken(unsigned variant, u16 word)
{
	s16 const value = word;
	switch (variant % 6)
	{
	case 0: return value >= 0; case 1: return value < 0; case 2: return value != 0;
	case 3: return value == 0; case 4: return value > 0; default: return value <= 0;
	}
}

void tms320c54x_test_state::start_block_bcd(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const unconditional = profile_is("rptb-bd");
	bool const tc = profile_is("rptb-tc-delay");
	bool const banz = profile_is("rptb-banz");
	if (unconditional || tc) variant *= 6;
	unsigned const count = std::array<unsigned, 4>{0, 1, 2, 7}[banz ? variant % 4 : (variant / 12) % 4];
	bool const b = !banz && variant % 12 >= 6, wide = !banz && (variant / 192) % 2;
	u32 const base = banz ? block_banz_case(variant).base : (variant / 384) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, 0xf272); program.write_word(base + 0x601, 0x610);
	program.write_word(base + 0x604, tc ? 0x3492 : b ? 0x1192 : 0x1092);
	program.write_word(base + 0x605, tc ? b ? 0xfa30 : 0xfa20 : unconditional ? 0xf273 : 0xfa42 + variant % 6 + (b ? 8 : 0)); program.write_word(base + 0x606, 0x60d);
	program.write_word(base + 0x607, wide ? b ? 0xf300 : 0xf000 : 0x6d94); program.write_word(base + 0x608, wide ? 1 : 0x6d95);
	if (tc && !wide) program.write_word(base + 0x607, b ? 0xf4bc : 0xf5bc);
	program.write_word(base + 0x609, 0x7693); program.write_word(base + 0x60a, 0x1111);
	program.write_word(base + 0x60b, 0xf073); program.write_word(base + 0x60c, 0x60f);
	program.write_word(base + 0x60d, 0x7693); program.write_word(base + 0x60e, 0x2222);
	program.write_word(base + 0x60f, 0x6d96); program.write_word(base + 0x611, 0x6d97); program.write_word(base + 0x612, 0xf4e1);
	if (banz)
	{
		auto const f = block_banz_case(variant);
		program.write_word(base + 0x605, 0x6ce0 + f.form * 8);
		program.write_word(base + 0x606, f.offset); program.write_word(base + 0x607, 0x60d);
		program.write_word(base + 0x608, 0x6d95);
	}
	for (unsigned i = 0; i <= count; ++i) { data.write_word(0x5000 + i, block_bcd_source(variant, i)); data.write_word(0x6000 + i, 0xdead); }
	m_cpu->set_pipeline_frontend(false);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_ST0, 0xa608}, {cpu_regs::STATE_ST1, 0x900}, {cpu_regs::STATE_TCR, 0x10},
		{cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0}, {cpu_regs::STATE_BRC, count}, {cpu_regs::STATE_A, 0x1234},
		{cpu_regs::STATE_B, 0x2345},
	});
	if (tc) m_cpu->set_state_int(tms320c54x_device::STATE_T, 0);
	for (unsigned ar = 2; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, ar == 2 ? 0x5000 : ar == 3 ? 0x6000 : 0x1000 + ar * 0x100);
	if (banz) { m_cpu->set_state_int(tms320c54x_device::STATE_AR0, block_banz_case(variant).initial); m_cpu->set_state_int(tms320c54x_device::STATE_BK, block_banz_case(variant).bk); }
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0; m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "block_bcd_read", [this, variant, count](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		unsigned const i = m_pipeline_operand_reads++; expect(i <= count && at == 0x5000 + i && word == block_bcd_source(variant, i) && m_cpu->state_int(tms320c54x_device::STATE_BRC) == count - i && bool(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x8000) == (i < count), "Delayed internal branches retain ordered sources and Decode-owned loop counters");
		auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(at, word ^ 0xffff);
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "block_bcd_write", [this, variant, count, unconditional, banz, tc, b](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; unsigned const i = m_pipeline_store_writes++; bool const taken = tc ? bool(block_bcd_source(variant, i) & 0x8000) == b : banz ? block_banz_resolved(variant, i) != 0 : unconditional || block_bcd_taken(variant, block_bcd_source(variant, i)); expect(i <= count && at == 0x6000 + i && word == (taken ? 0x2222 : 0x1111), "Internal branch publishes only its selected arm"); });
	m_pipeline_fetch_tap = program.install_read_tap(base + (banz ? 0x607 : 0x606), base + (banz ? 0x607 : 0x606), "block_bcd_target", [this, count](offs_t, u16 &word, u16) { if (!machine().side_effects_disabled()) { expect(word == 0x60d && m_pipeline_extensions_reads <= count, "Internal branch captures its target exactly once per iteration"); ++m_pipeline_extensions_reads; } });
	m_phase = 255560; m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}


bool tms320c54x_test_state::check_delay_dld_results()
{
	if (m_phase == 255260)
	{
		bool const shifted = profile_is("shift-delay");
		bool const shift_alu = profile_is("shift-alu-delay");
		bool const add = profile_is("add-delay");
		bool const dld = profile_is("dld-delay");
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = shift_alu ? delay_shift_alu_case(variant) : dld ? delay_dld_case(variant) : add ? delay_add_case(variant) : delay_load_case(variant, shifted);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_A) == (tick > f.read && !f.destination ? f.result : f.a) && m_cpu->state_int(tms320c54x_device::STATE_B) == (tick > f.read && f.destination ? f.result : f.b) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == ((add || shift_alu ? tick > f.read : tick >= f.read - 1) ? f.final_st0 : f.st0) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5aa5, "Delay arithmetic/load publishes its retained result and owned flags in Execute");
		for (unsigned i = 0; i < 8; ++i) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == (add || tick < f.read - 1 ? f.initial[i] : tick == f.read - 1 ? f.final[i] : 0x2200 + i), "Delay addressing preserves or updates the selected pointer once");
		if (tick == f.literal_clock()) { auto const disabled = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.caller + 0x601 + f.delay(), 0xdead); }
		if (!add && tick == f.read - 1) for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, 0x2200 + i);
		if (tick == f.read && (!add || f.address >= 0x60)) { auto const disabled = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.address, 0xdead); if (dld) m_cpu->space(AS_DATA).write_word(f.address ^ 1, 0xbeef); }
		capture_pipeline_at(f.checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_pipeline_register_writes, &m_fast_stack_reads, &m_pipeline_extensions_reads},
			"Save delayed LD around literal Access and Read ownership");
		if (tick == 24)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == (f.kind == 2 ? f.caller + 0x606 : f.target + 0x702) && m_pipeline_operand_reads == (dld ? 2U : 1U) && m_pipeline_store_writes == 1 && m_pipeline_register_writes == (f.kind == 4 ? 2U : unsigned(f.kind == 3)) && m_fast_stack_reads == unsigned(f.kind == 5) && m_pipeline_extensions_reads == 1 && m_cpu->state_int(tms320c54x_device::STATE_TRN) == u16(f.result) && m_cpu->state_int(tms320c54x_device::STATE_SP) == (f.kind == 4 ? 0x2ffe : f.kind == 3 ? 0x2fff : f.kind == 5 ? 0x3001 : 0x3000), "Delayed LD reaches the selected target with correct page frame and observed data");
			if (replay_pipeline_at(255270, f.checkpoint,
				"Restore delayed LD without duplicate data literal or frame transactions")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (shift_alu ? 442367U : add ? 18431U : shifted ? 110591U : 147455U)) { start_delay_load(variant + 1); return true; }
			if (shift_alu) { osd_printf_info("TMS320C54x delayed shifted ALU: PASS cases=442368 restore=442368 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD forms=DP,indirect,postincrement operations=ADD,SUB source_destination_pairs=4 shifts=32 memory_values=4 sxm_ovm=4 pages=2 snapshots=3 source_reads=1 extension_fetches=1 pointer_updates=once target_observes_result=1 interrupt_composition=unvalidated physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delayed shifted ALU complete"); }
			if (dld) { osd_printf_info("TMS320C54x delayed DLD: PASS cases=147456 restore=147456 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD forms=offset,preincrement,circular,absolute registers=8 offsets=-1,2 parity=even,odd destinations=2 patterns=4 sxm_c16=4 compatibility_states=2 pages=2 snapshots=3 ordered_reads=2 literal_fetches=1 pointer_updates=once target_observes_result=1 interrupt_repeat_composition=unvalidated physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delayed DLD complete"); }
			if (add) { osd_printf_info("TMS320C54x delayed absolute ADD: PASS cases=18432 restore=18432 controls=BD,CALLD,FCALLD,RETD sources=RAM,AR3,AL,AH,BL,BH destinations=2 memory_values=4 initial_values=4 sxm_ovm=4 pages=2 snapshots=3 source_reads=1 literal_fetches=1 call_frames=owned target_observes_result=1 conditional_composition=unvalidated physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delayed absolute ADD complete"); }
			if (shifted) { osd_printf_info("TMS320C54x delayed shifted LD: PASS cases=110592 restore=110592 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD forms=DP,indirect,postincrement destinations=2 shifts=32 memory_values=4 sxm_ovm=4 pages=2 snapshots=3 source_reads=1 extension_fetches=1 pointer_updates=once target_observes_result=1 interrupt_composition=unvalidated physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delayed shifted LD complete"); }
			osd_printf_info("TMS320C54x delayed LD: PASS cases=147456 restore=147456 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD forms=offset,preincrement,circular,absolute registers=8 offsets=2 destinations=2 memory_values=4 sxm_ovm=4 compatibility_states=2 pages=2 snapshots=3 source_reads=1 literal_fetches=1 pointer_updates=once target_observes_result=1 interrupt_composition=unvalidated physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated delayed LD complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255270) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255260; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

} // namespace c54x_test
