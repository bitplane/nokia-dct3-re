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


bool tms320c54x_test_state::check_delay_arithmetic_results()
{
	if (m_phase == 255260)
	{
		bool const shift_alu = profile_is("shift-alu-delay");
		bool const add = profile_is("add-delay");
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = shift_alu ? delay_shift_alu_case(variant) : add ? delay_add_case(variant) : delay_shift_load_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_A) == (tick > f.read && !f.destination ? f.result : f.a) && m_cpu->state_int(tms320c54x_device::STATE_B) == (tick > f.read && f.destination ? f.result : f.b) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == ((add || shift_alu ? tick > f.read : tick >= f.read - 1) ? f.final_st0 : f.st0) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5aa5, "Delay arithmetic/load publishes its retained result and owned flags in Execute");
		for (unsigned i = 0; i < 8; ++i) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == (add || tick < f.read - 1 ? f.initial[i] : tick == f.read - 1 ? f.final[i] : 0x2200 + i), "Delay addressing preserves or updates the selected pointer once");
		if (tick == f.literal_clock()) { auto const disabled = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.caller + 0x601 + f.delay(), 0xdead); }
		if (!add && tick == f.read - 1) for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, 0x2200 + i);
		if (tick == f.read && (!add || f.address >= 0x60)) { auto const disabled = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.address, 0xdead); }
		capture_pipeline_at(f.checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_pipeline_register_writes, &m_fast_stack_reads, &m_pipeline_extensions_reads},
			"Save delayed LD around literal Access and Read ownership");
		if (tick == 24)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == (f.kind == 2 ? f.caller + 0x606 : f.target + 0x702) && m_pipeline_operand_reads == 1U && m_pipeline_store_writes == 1 && m_pipeline_register_writes == (f.kind == 4 ? 2U : unsigned(f.kind == 3)) && m_fast_stack_reads == unsigned(f.kind == 5) && m_pipeline_extensions_reads == 1 && m_cpu->state_int(tms320c54x_device::STATE_TRN) == u16(f.result) && m_cpu->state_int(tms320c54x_device::STATE_SP) == (f.kind == 4 ? 0x2ffe : f.kind == 3 ? 0x2fff : f.kind == 5 ? 0x3001 : 0x3000), "Delayed LD reaches the selected target with correct page frame and observed data");
			if (replay_pipeline_at(255270, f.checkpoint,
				"Restore delayed LD without duplicate data literal or frame transactions")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (shift_alu ? 442367U : add ? 18431U : 110591U)) { start_delay_load(variant + 1); return true; }
			if (shift_alu) { osd_printf_info("TMS320C54x delayed shifted ALU: PASS cases=442368 restore=442368 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD forms=DP,indirect,postincrement operations=ADD,SUB source_destination_pairs=4 shifts=32 memory_values=4 sxm_ovm=4 pages=2 snapshots=3 source_reads=1 extension_fetches=1 pointer_updates=once target_observes_result=1 interrupt_composition=unvalidated physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delayed shifted ALU complete"); }
			if (add) { osd_printf_info("TMS320C54x delayed absolute ADD: PASS cases=18432 restore=18432 controls=BD,CALLD,FCALLD,RETD sources=RAM,AR3,AL,AH,BL,BH destinations=2 memory_values=4 initial_values=4 sxm_ovm=4 pages=2 snapshots=3 source_reads=1 literal_fetches=1 call_frames=owned target_observes_result=1 conditional_composition=unvalidated physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delayed absolute ADD complete"); }
			{ osd_printf_info("TMS320C54x delayed shifted LD: PASS cases=110592 restore=110592 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD forms=DP,indirect,postincrement destinations=2 shifts=32 memory_values=4 sxm_ovm=4 pages=2 snapshots=3 source_reads=1 extension_fetches=1 pointer_updates=once target_observes_result=1 interrupt_composition=unvalidated physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delayed shifted LD complete"); }
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255270) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255260; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

} // namespace c54x_test
