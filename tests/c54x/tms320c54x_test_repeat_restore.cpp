// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

tms320c54x_test_state::fcala_delay_fixture tms320c54x_test_state::fcala_delay_case(unsigned v)
{
	return {bool(v & 1), bool((v / 18) % 2), (v / 36) % 3,
		std::array<unsigned, 6>{3, 6, 7, 8, 10, 12}[v / 108],
		u32(std::array<unsigned, 3>{0, 2, 0x7f}[(v / 2) % 3] << 16),
		u32(std::array<unsigned, 3>{0, 3, 0x7f}[(v / 6) % 3] << 16)};
}

void tms320c54x_test_state::start_fcala_delay(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_fcala_delay.variant = variant; auto const f = fcala_delay_case(variant);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) { program.write_word(f.caller + 0x600 + i, 0xf495); program.write_word(f.target + 0x700 + i, 0xf495); }
	program.write_word(f.caller + 0x600, f.b ? 0xf7e7 : 0xf6e7);
	program.write_word(f.caller + 0x601, f.wide ? 0x7693 : f.b ? 0x890e : 0x880e);
	program.write_word(f.caller + 0x602, f.wide ? 0x5aa5 : f.b ? 0x890f : 0x880f);
	program.write_word(f.caller + 0x603, 0xf4e1);
	program.write_word(f.target + 0x700, f.b ? 0x8192 : 0x8092); program.write_word(f.target + 0x708, 0xf4e4);
	unsigned const vector = f.source == 2 ? 4 : 0x40; program.write_word(f.target + vector, 0xf4eb);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, f.source == 1 ? 1 : 0);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.source == 1 ? 0x100 : 0x900);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x1500); m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x1400); m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x4400);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.b ? 0x1234 : 0x5500000700ULL | f.target);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b ? 0x5500000700ULL | f.target : 0x2345);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x5aa5); m_cpu->set_state_int(tms320c54x_device::STATE_TRN, 0x2468); data.write_word(0x1400, 0xdead);
	m_fcala_delay.observed = {}; m_fcala_delay.replay = false; m_fcala_delay.tick = 0;
	// Retire predecessor observers before installing this family's taps.
	m_pipeline_operand_tap.remove();
	m_pipeline_write_tap.remove();
	m_pipeline_fetch_tap.remove();
	m_vector_prefetch_tap.remove();
	m_fcala_delay.write_tap = data.install_write_tap(0, 0xffff, "fcala_delay_writes", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		auto &c = m_fcala_delay.observed;
		if (at == 0xfff || at == 0xffe) { unsigned const i = c.frames++; expect(i < 2 && at == 0xfff - i && word == (i ? f.caller >> 16 : 0x603) && m_cpu->pipeline_cycles() == 5 + i, "FCALAD owns PC+3 then caller XPC without a nondelayed hold"); return; }
		if (at == 0xffd) { expect(f.source && c.frames == 2 && word >= 0x700 && word <= 0x708, "Interrupt frame follows the complete far-call frame"); c.irq_pc = word; return; }
		if (at == 0x1500) { expect(!c.callee && word == 0xbeef && c.delays == (f.wide ? 1U : 2U) && (f.source ? m_cpu->pipeline_cycles() >= 10 : m_cpu->pipeline_cycles() == 10), "Target executes after the four-cycle call and complete delay work"); ++c.callee; return; }
		unsigned const i = c.delays++; expect(i < (f.wide ? 1U : 2U) && at == (f.wide ? 0x1400 : 0x0e + i) && word == (f.wide ? 0x5aa5 : 0xbeef) && m_cpu->pipeline_cycles() == (f.wide ? 8 : 7 + i), "Both delay words execute once after the retained accumulator target");
	});
	m_fcala_delay.read_tap = data.install_read_tap(0, 0xffff, "fcala_delay_reads", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		auto &c = m_fcala_delay.observed;
		if (at == 0xffd) { expect(f.source && !c.irq_reads && word == c.irq_pc, "RETE consumes only its own interrupt frame"); ++c.irq_reads; return; }
		unsigned const i = c.reads++; expect(i < 2 && at == 0xffe + i && word == (i ? 0x603 : f.caller >> 16), "FRET balances the retained PC+3 and XPC frame");
	});
	m_fcala_delay.fetch_tap = program.install_read_tap(f.target + 0x700, f.target + 0x700, "fcala_delay_target", [this, f](offs_t, u16 &, u16) {
		if (machine().side_effects_disabled()) return;
		auto &c = m_fcala_delay.observed;
		expect(c.target_fetches ? f.source && c.frames == 2 && c.target_fetches == 1 : c.frames == 1 && m_cpu->pipeline_cycles() == 6, "FCALAD target Fetch follows Read; only an interrupt may refetch it"); ++c.target_fetches;
	});
	m_fcala_delay.vector_tap = program.install_read_tap(f.target + vector, f.target + vector, "fcala_delay_vector", [this, f](offs_t, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		auto &c = m_fcala_delay.observed;
		expect(f.source && !c.vectors && word == 0xf4eb && c.frames == 2 && c.delays == (f.wide ? 1U : 2U), "IRQ/NMI cannot enter before all delay effects and call-frame words"); ++c.vectors; m_cpu->set_input_line(f.source == 2 ? INPUT_LINE_NMI : 0, CLEAR_LINE);
	});
	m_phase = 255820; m_cpu->set_state_int(STATE_GENPC, f.caller + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

bool tms320c54x_test_state::check_fcala_delay_results()
{
	if (m_phase == 255820)
	{
		unsigned const variant = m_fcala_delay.variant, tick = ++m_fcala_delay.tick; auto const f = fcala_delay_case(variant); auto const &c = m_fcala_delay.observed;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Delayed accumulator far call and its complete delay words remain admitted");
		if (tick == 5)
		{
			m_cpu->set_state_int(f.b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A, 0xdeadbeef);
			if (f.source) m_cpu->set_input_line(f.source == 2 ? INPUT_LINE_NMI : 0, ASSERT_LINE);
		}
		if (tick == f.checkpoint && !m_fcala_delay.replay) { m_fcala_delay.snapshot = c; expect(capture_snapshot(machine(), m_fcala_delay.saved) == STATERR_NONE, "Save delayed accumulator target frame delays and settled pending interrupt"); }
		if (tick == 64)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.caller + 0x604 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && c.frames == 2 && c.reads == 2 && c.delays == (f.wide ? 1U : 2U) && c.callee == 1 && c.vectors == unsigned(bool(f.source)) && c.irq_reads == unsigned(bool(f.source)) && c.target_fetches == 1 + unsigned(f.source && c.irq_pc == 0x700), "FCALAD reaches its retained target, retires delay work once and balances both call and interrupt frames");
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (f.b ? 0x1234 : 0xdeadbeef) && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.b ? 0xdeadbeef : 0x2345) && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x1501 && m_cpu->state_int(tms320c54x_device::STATE_AR3) == (f.wide ? 0x1401 : 0x1400) && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x4400 && m_cpu->state_int(tms320c54x_device::STATE_T) == (f.wide ? 0x5aa5 : 0xbeef) && m_cpu->state_int(tms320c54x_device::STATE_TRN) == (f.wide ? 0x2468 : 0xbeef) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (f.source ? 0x100 : 0x900), "FCALAD preserves flags and uses the accumulator value sampled before delay effects");
			if (!m_fcala_delay.replay)
			{
				m_fcala_delay.replay = true; m_fcala_delay.tick = f.checkpoint; m_fcala_delay.observed = m_fcala_delay.snapshot;
				expect(restore_snapshot(machine(), *m_check_timer, m_fcala_delay.saved) == STATERR_NONE, "Restore delayed far call without duplicate target frame or IRQ"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 255830; m_check_timer->enable(true); return true;
			}
			m_fcala_delay.read_tap.remove(); m_fcala_delay.write_tap.remove(); m_fcala_delay.fetch_tap.remove(); m_fcala_delay.vector_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 647) { start_fcala_delay(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged FCALAD: PASS cases=648 restore=648 accumulators=2 caller_pages=3 target_pages=3 delay_layouts=2 sources=none,IRQ,NMI snapshots=6 target_sample=Read return=PC_plus_3 frames=balanced delay_effects=once nominal_callee_execute=10 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated staged FCALAD complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255830) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255820; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true; }
	return false;
}

} // namespace c54x_test
