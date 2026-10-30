// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

tms320c54x_test_state::repeat_context_fixture tms320c54x_test_state::repeat_context_case(unsigned v)
{
	return {bool(v % 2), bool((v / 8) % 2), std::array<unsigned, 4>{0, 1, 2, 7}[(v / 2) % 4],
		std::array<unsigned, 6>{4, 12, 18, 22, 26, 34}[v / 64], (v / 16) % 2 ? 0x20000U : 0U, u16(0x3000 + (v / 32) % 2)};
}

u16 tms320c54x_test_state::repeat_context_word(repeat_context_fixture const &f, unsigned part) const
{
	return part == 0 ? f.end() : part == 1 ? f.start() : f.replace ? f.count : f.count - (m_repeat_context.observed.body_reads - 1);
}

void tms320c54x_test_state::start_repeat_context(unsigned variant)
{
	bool const pmst = profile_is("repeat-pmst");
	bool const indirect = profile_is("repeat-popd");
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_repeat_context.variant = variant;
	auto const f = repeat_context_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) { program.write_word(f.base + 0x600 + i, 0xf495); program.write_word(f.base + 0x700 + i, 0xf495); program.write_word(f.base + f.start() + i, 0xf495); }
	program.write_word(f.base + 0x600, 0xf272); program.write_word(f.base + 0x601, f.old_end());
	program.write_word(f.base + 0x604, 0xe589); program.write_word(f.base + 0x605, 0xe800);
	program.write_word(f.base + 0x606, 0xfb45); program.write_word(f.base + 0x607, 0x700);
	program.write_word(f.base + 0x60a, 0x6d94); program.write_word(f.base + f.old_end() + 1, 0x6d97); program.write_word(f.base + f.old_end() + 2, 0xf4e1);
	if (f.replace)
	{
		// A synthetic CPU context-switch program, not a handset fixture:
		// restore a RAM frame and replace the return PC through an actual ST.
		program.write_word(f.base + 0x700, 0xeefd);
		program.write_word(f.base + 0x701, 0x8a1c); program.write_word(f.base + 0x702, 0x8a1b); program.write_word(f.base + 0x703, 0x8a1a);
		program.write_word(f.base + 0x704, 0x76f8); program.write_word(f.base + 0x705, f.sp - 1); program.write_word(f.base + 0x706, f.start());
		program.write_word(f.base + 0x70d, 0xfc00);
		data.write_word(f.sp - 4, f.end()); data.write_word(f.sp - 3, f.start()); data.write_word(f.sp - 2, f.count);
		program.write_word(f.base + f.start(), 0xe589); program.write_word(f.base + f.start() + 1, 0x6d94);
		program.write_word(f.base + f.end() + 1, 0x6d97); program.write_word(f.base + f.end() + 2, 0xf4e1);
	}
	else
	{
		program.write_word(f.base + 0x700, 0x4a1a); program.write_word(f.base + 0x701, 0x4a1b); program.write_word(f.base + 0x702, 0x4a1c);
		program.write_word(f.base + 0x703, 0x8a1c); program.write_word(f.base + 0x704, 0x8a1b); program.write_word(f.base + 0x705, 0x8a1a);
		program.write_word(f.base + 0x70c, 0xfc00);
	}
	if (indirect)
	{
		unsigned const first = f.replace ? 0x701 : 0x705;
		if (!f.replace)
		{
			program.write_word(f.base + 0x703, 0xf495); program.write_word(f.base + 0x704, 0xf495);
			program.write_word(f.base + 0x70c, 0xf495); program.write_word(f.base + 0x70e, 0xfc00);
		}
		program.write_word(f.base + first, 0x8b80);
		program.write_word(f.base + first + 1, 0x8b81);
		program.write_word(f.base + first + 2, 0x8b85);
	}
	if (pmst)
	{
		program.write_word(f.base + (f.replace ? 0x70d : 0x70c), 0xf495);
		program.write_word(f.base + 0x70e, 0x4a1d);
		program.write_word(f.base + 0x711, 0x8b80);
		program.write_word(f.base + 0x718, 0xfc00);
	}
	// Six NOP words after the BRC pop separate restoration from a loop
	// endpoint (SPRU131G table 7-25); shorter gaps are not accepted here.
	for (unsigned i = 0; i < 16; ++i) { data.write_word(0x5000 + i, 0x1357 ^ (i * 0x2345)); data.write_word(0x6000 + i, 0xdead); }
	m_cpu->set_pipeline_frontend(false);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900); m_cpu->set_state_int(tms320c54x_device::STATE_SP, f.sp);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC, f.replace ? 3 : f.count); m_cpu->set_state_int(tms320c54x_device::STATE_A, 0); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfedcba9876ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x5000); m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x6000); m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x1400); m_cpu->set_state_int(tms320c54x_device::STATE_AR7, 0x1700);
	if (pmst) m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x1d);
	if (indirect)
	{
		m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x1c);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x1b);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x1a);
	}
	m_repeat_context.observed = {}; m_repeat_context.replay = false; m_repeat_context.tick = 0;
	// Retire predecessor observers before installing this family's taps.
	m_pipeline_operand_tap.remove();
	m_pipeline_write_tap.remove();
	m_repeat_context.read_tap = data.install_read_tap(0, 0xffff, "repeat_context_reads", [this, f, pmst](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		auto &c = m_repeat_context.observed;
		if (pmst && at == 0x1d) { expect(!word && c.pmst_reads == c.pmst_writes, "PSHM captures unchanged PMST once per active-context call"); ++c.pmst_reads; return; }
		if (pmst && at == f.sp - 2 && c.pmst_saves > c.pmst_pops) { expect(!word, "POPD captures the saved PMST once"); ++c.pmst_pops; auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(at, 0xffff); return; }
		if (at >= 0x1a && at <= 0x1c) { unsigned const part = c.mapped_reads++ % 3; expect(!f.replace && at == 0x1a + part && word == repeat_context_word(f, 2 - part), "PSHM captures the actual current repeat context in order"); return; }
		if (at == f.sp - 1) { expect(c.frame_reads < f.calls() && word == (f.replace ? f.start() : 0x60a), "Near return captures the CPU-published context continuation once"); ++c.frame_reads; auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(at, 0xdead); return; }
		if (at >= f.sp - 4 && at <= f.sp - 2) { unsigned const part = c.restore_reads++ % 3; expect(at == f.sp - 4 + part && word == repeat_context_word(f, part), "POPM reads one retained REA RSA BRC frame in order"); auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(at, 0xdead); return; }
		unsigned const i = c.body_reads++; expect(i < f.copies() && at == 0x5000 + i && word == u16(0x1357 ^ (i * 0x2345)), "Restored repeat context owns each ordered body source once"); });
	m_repeat_context.write_tap = data.install_write_tap(0, 0xffff, "repeat_context_writes", [this, f, pmst](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		auto &c = m_repeat_context.observed;
		if (pmst && at == 0x1d) { expect(!word && c.pmst_writes < c.pmst_pops, "POPD publishes retained PMST once outside the active body"); ++c.pmst_writes; return; }
		if (pmst && at == f.sp - 2 && c.pmst_reads > c.pmst_saves) { expect(!word, "PSHM publishes its retained PMST frame once"); ++c.pmst_saves; return; }
		if (at >= 0x1a && at <= 0x1c) { unsigned const part = c.control_writes++ % 3; expect(at == 0x1c - part && word == repeat_context_word(f, part) && c.control_writes <= c.restore_reads, "POPM publishes the captured repeat registers once in order"); return; }
		if (at == f.sp - 1) { bool const replacement = f.replace && c.frame_writes; expect(c.frame_writes < (f.replace ? 2U : f.calls()) && word == (replacement ? f.start() : 0x60a), "Only CPU call and explicit context-switch ST publish the return frame"); ++c.frame_writes; return; }
		if (at >= f.sp - 4 && at <= f.sp - 2) { unsigned const part = c.saved_writes++ % 3; expect(!f.replace && at == f.sp - 2 - part && word == repeat_context_word(f, 2 - part), "PSHM saves BRC RSA REA once before their corresponding pops"); return; }
		unsigned const i = c.body_writes++; expect(i < f.copies() && at == 0x6000 + i && word == u16(0x1357 ^ (i * 0x2345)), "Restored context owns each ordered body destination once"); });
	m_phase = 255480; m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

bool tms320c54x_test_state::check_repeat_context_results()
{
	if (m_phase == 255480)
	{
		bool const pmst = profile_is("repeat-pmst");
		bool const indirect = profile_is("repeat-popd");
		unsigned const variant = m_repeat_context.variant, tick = ++m_repeat_context.tick; auto const f = repeat_context_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Repeat-context restoration outside the loop body remains admitted");
		if (tick == f.checkpoint && !m_repeat_context.replay) { m_repeat_context.snapshot = m_repeat_context.observed; expect(capture_snapshot(machine(), m_repeat_context.saved) == STATERR_NONE, "Save active repeat context around push pop publication and return"); }
		if (tick == 600)
		{
			auto const &c = m_repeat_context.observed; unsigned const calls = f.calls();
			if (pmst) expect(c.pmst_reads == calls && c.pmst_saves == calls && c.pmst_pops == calls && c.pmst_writes == calls && !m_cpu->state_int(tms320c54x_device::STATE_PMST), "Active-repeat PMST restoration preserves one complete stack transaction per call");
			if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE) || m_cpu->state_int(STATE_GENPC) != f.base + f.end() + 3 || c.body_reads != f.copies())
				osd_printf_info("Repeat-context mismatch: case=%u replay=%u replace=%u pc=%06x brc=%04x rsa=%04x rea=%04x st1=%04x reads=%u writes=%u restore_reads=%u publications=%u\n", variant, unsigned(m_repeat_context.replay), unsigned(f.replace), unsigned(m_cpu->state_int(STATE_GENPC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_BRC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_RSA)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_REA)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)), c.body_reads, c.body_writes, c.restore_reads, c.control_writes);
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + f.end() + 3 && !m_cpu->state_int(tms320c54x_device::STATE_BRC) && m_cpu->state_int(tms320c54x_device::STATE_RSA) == f.start() && m_cpu->state_int(tms320c54x_device::STATE_REA) == f.end() &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x900 && m_cpu->state_int(tms320c54x_device::STATE_SP) == f.sp && m_cpu->state_int(tms320c54x_device::STATE_A) == 0 && m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfedcba9876ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x5000 + f.copies() && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x6000 + f.copies() && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x1400 + f.count + 1 && m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0x1701 &&
				c.body_reads == f.copies() && c.body_writes == f.copies() && c.mapped_reads == (f.replace ? 0 : 3 * calls) && c.saved_writes == (f.replace ? 0 : 3 * calls) && c.restore_reads == 3 * calls && c.control_writes == 3 * calls && c.frame_reads == calls && c.frame_writes == (f.replace ? 2 : calls),
				"Same or replaced active repeat context resumes its actual saved registers with ordered copies and balanced frames");
			{ auto const disable = machine().disable_side_effects(); for (unsigned i = 0; i < f.copies(); ++i) expect(m_cpu->space(AS_DATA).read_word(0x6000 + i) == u16(0x1357 ^ (i * 0x2345)), "Restored repeat context final buffer matches independent oracle"); }
			if (!m_repeat_context.replay) { m_repeat_context.replay = true; m_repeat_context.tick = f.checkpoint; m_repeat_context.observed = m_repeat_context.snapshot; expect(restore_snapshot(machine(), *m_check_timer, m_repeat_context.saved) == STATERR_NONE, "Restore suspended repeat context without duplicate stack or body work"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 255490; m_check_timer->enable(true); return true; }
			m_repeat_context.read_tap.remove(); m_repeat_context.write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 383) { start_repeat_context(variant + 1); return true; }
			if (indirect)
			{
				osd_printf_info("TMS320C54x active repeat POPD context: PASS cases=384 restore=384 contexts=same,replaced counts=0,1,2,7 layouts=2 pages=2 stack_alignments=2 snapshots=6 control_order=REA,RSA,BRC source_retained=1 publications=once actual_iterations=1 interrupt_composition=unvalidated physical_phase_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated active repeat POPD context complete");
			}
			if (pmst)
			{
				osd_printf_info("TMS320C54x active repeat POPD PMST: PASS cases=384 restore=384 contexts=same,replaced counts=0,1,2,7 layouts=2 pages=2 stack_alignments=2 snapshots=6 pmst_unchanged=1 stack_retained=1 publications=once actual_iterations=1 interrupt_composition=unvalidated physical_phase_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated active repeat POPD PMST complete");
			}
			osd_printf_info("TMS320C54x active repeat context: PASS cases=384 restore=384 modes=same,replaced counts=0,1,2,7 layouts=2 pages=2 stack_alignments=2 snapshots=6 control_order=REA,RSA,BRC source_reads=once publications=once source_mutation_after_read=1 ordered_copies=1 balanced_frames=1 brc_gap_words=6 interrupt_composition=unvalidated physical_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated active repeat context complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255490) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255480; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

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
