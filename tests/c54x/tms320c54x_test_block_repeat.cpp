// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

void tms320c54x_test_state::start_rptbd(unsigned variant)
{
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove();
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_rptbd.variant = variant;
	u32 const base = rptbd_base(variant); unsigned const layout = rptbd_layout(variant);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, 0xf272); program.write_word(base + 0x601, 0x606);
	program.write_word(base + 0x602, std::array<u16, 4>{0x6d95, 0x770e, 0x10e1, 0x6deb}[layout]);
	program.write_word(base + 0x603, std::array<u16, 4>{0x6d96, 0x1234, 3, 0x0a40}[layout]);
	program.write_word(base + 0x604, 0xe589); program.write_word(base + 0x605, 0x6d94);
	// Keep the IRQ fixture's terminal IDLE beyond the interrupted body;
	// this tests loop return, not the separately covered IDLE wake path.
	program.write_word(base + (variant >= 512 ? 0x60f : 0x607), 0xf4e1);
	if (variant >= 512)
	{
		u32 const vector = base + (rptbd_irq_line(variant) == INPUT_LINE_NMI ? 4 : 0x48);
		program.write_word(vector, 0xf495); program.write_word(vector + 1, 0xf4eb);
	}
	for (unsigned i = 0; i < 8; ++i) { data.write_word(0x5000 + i, rptbd_source(variant, i)); data.write_word(0x6000 + i, 0xdead); }
	data.write_word(0x4003, rptbd_source(variant, 0));
	m_cpu->set_pipeline_frontend(false);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_ST0, 0xa608}, {cpu_regs::STATE_ST1, 0x900}, {cpu_regs::STATE_IFR, 0},
		{cpu_regs::STATE_IMR, 0}, {cpu_regs::STATE_TCR, 0x10},
	});
	m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	if (variant >= 512) { m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x100); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, rptbd_irq_line(variant) == 2 ? 4 : 0); }
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xfedcba9876ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x5aa5); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x4000); m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x5000);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR3, layout == 3 ? 0x55c0 : 0x6000); m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x7000);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x7100); m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x7200);
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC, rptbd_count(variant));
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_rptbd.observed = {}; m_rptbd.replay = false; m_rptbd.tick = 0;
	m_rptbd.fetch_tap = program.install_read_tap(base + 0x601, base + 0x603, "rptbd_retained_words",
		[this, base, layout](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			unsigned const i = m_rptbd.observed.fetches++;
			u16 const expected = i == 0 ? 0x606 : i == 1 ? std::array<u16, 4>{0x6d95, 0x770e, 0x10e1, 0x6deb}[layout] : std::array<u16, 4>{0x6d96, 0x1234, 3, 0x0a40}[layout];
			expect(i < 3 && at == base + 0x601 + i && word == expected, "RPTBD fetches endpoint and both delay words exactly once"); });
	m_rptbd.read_tap = data.install_read_tap(0, 0xffff, "rptbd_ordered_reads",
		[this, variant, layout](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			if (at == 0x2fff && variant >= 512) { expect(!m_rptbd.observed.stack_reads && m_rptbd.observed.stack_writes == 1 && word == m_rptbd.return_pc, "RPTBD ISR reads its original continuation frame once"); ++m_rptbd.observed.stack_reads; return; }
			if (at == 0x4003) { expect(layout == 2 && !m_rptbd.observed.delay_reads && word == rptbd_source(variant, 0), "RPTBD delay load samples its operand once"); ++m_rptbd.observed.delay_reads; return; }
			unsigned const i = m_rptbd.observed.reads++;
			expect(i <= rptbd_count(variant) && at == 0x5000 + i && word == rptbd_source(variant, i), "RPTBD repeats ordered source reads, excluding delay work"); });
	m_rptbd.write_tap = data.install_write_tap(0, 0xffff, "rptbd_ordered_writes",
		[this, variant, layout](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			if (at == 0x2fff && variant >= 512) { expect(!m_rptbd.observed.stack_writes && word >= 0x604 && word <= 0x60f, "RPTBD interrupt stacks one continuation after the complete delay span"); ++m_rptbd.observed.stack_writes; m_rptbd.return_pc = word; return; }
			if (at == 0x0e) { expect(layout == 1 && !m_rptbd.observed.delay_writes && word == 0x1234, "RPTBD delay STM publishes T once"); ++m_rptbd.observed.delay_writes; return; }
			unsigned const i = m_rptbd.observed.writes++;
			expect(i <= rptbd_count(variant) && at == 0x6000 + i && word == rptbd_source(variant, i), "RPTBD repeats ordered destination writes without replay duplicates");
			if (!i && variant >= 512 && rptbd_late_irq(variant)) m_cpu->set_input_line(rptbd_irq_line(variant), ASSERT_LINE); });
	m_phase = 255320; m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_rptbd_edge(unsigned variant)
{
	start_rptbd(2 + ((variant / 7) % 2) * 64);
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_rptbd.variant = variant;
	m_rptbd.read_tap.remove(); m_rptbd.write_tap.remove(); m_rptbd.fetch_tap.remove();
	unsigned const kind = variant % 7; u32 const base = (variant / 7) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(base + 0x601, kind == 5 ? 0x605 : 0x606);
	program.write_word(base + 0x602, std::array<u16, 7>{0x771a, 0x881a, 0x8a1a, 0xf073, 0xf495, 0xf495, 0xf272}[kind]);
	program.write_word(base + 0x603, kind == 4 ? 0x10e1 : kind == 0 ? 1 : kind == 3 || kind == 6 ? 0x604 : 0xf495);
	program.write_word(base + 0x604, kind == 5 ? 0xf272 : kind == 4 ? 3 : 0xf495);
	program.write_word(base + 0x605, kind == 5 ? 0x606 : 0xf495);
	m_rptbd.read_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "rptbd_edge_no_reads", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_rptbd.observed.reads; });
	m_rptbd.write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "rptbd_edge_no_writes", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_rptbd.observed.writes; });
	m_phase = 255340; m_cpu->resume(SUSPEND_REASON_DISABLE);
}

bool tms320c54x_test_state::check_rptbd_layout_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 255320)
	{
		unsigned const variant = m_rptbd.variant, tick = ++m_rptbd.tick, layout = rptbd_layout(variant), iterations = rptbd_count(variant) + 1;
		bool const interrupt = variant >= 512;
		unsigned const checkpoint = interrupt ? std::array<unsigned, 6>{3, 4, 6, 8, 11, 17}[(variant - 512) / 128] : std::array<unsigned, 4>{3, 6, 8, 13}[variant / 128];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "RPTBD fixture remains in supported staged forms");
		// Replace fetched words and sampled RAM, never firmware architectural state.
		if (tick == 3) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(rptbd_base(variant) + 0x601, 0xdead); }
		if (tick == 5) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(rptbd_base(variant) + 0x603, 0xdead); }
		if (tick == 8 && layout == 2) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x4003, 0xdead); }
		if (interrupt && tick == 5 && !rptbd_late_irq(variant)) m_cpu->set_input_line(rptbd_irq_line(variant), ASSERT_LINE);
		if (tick == checkpoint && !m_rptbd.replay)
		{
			m_rptbd.snapshot = m_rptbd.observed; m_rptbd.saved.str({}); m_rptbd.saved.clear();
			m_rptbd.saved_return_pc = m_rptbd.return_pc;
			expect(machine().save().write_stream(m_rptbd.saved) == STATERR_NONE, "Save RPTBD header, delay span or active loop");
		}
		if (tick == (interrupt ? 160 : 96))
		{
			u64 const a = layout == 2 ? u64(s64(s16(rptbd_source(variant, 0)))) & 0xffffffffffULL : 0x123456789aULL;
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == rptbd_base(variant) + (interrupt ? 0x610 : 0x608) &&
				!m_cpu->state_int(tms320c54x_device::STATE_BRC) && m_cpu->state_int(tms320c54x_device::STATE_RSA) == 0x604 && m_cpu->state_int(tms320c54x_device::STATE_REA) == 0x606 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (interrupt ? 0x100 : 0x900) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x4000 && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x5000 + iterations && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x6000 + iterations &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x7000 + iterations && m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x7100 + unsigned(layout == 0) && m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x7200 + unsigned(layout == 0) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == a && m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfedcba9876ULL && m_cpu->state_int(tms320c54x_device::STATE_T) == (layout == 1 ? 0x1234 : 0x5aa5) &&
				m_rptbd.observed.reads == iterations && m_rptbd.observed.writes == iterations && m_rptbd.observed.fetches == 3 && m_rptbd.observed.delay_reads == unsigned(layout == 2) && m_rptbd.observed.delay_writes == unsigned(layout == 1),
				"RPTBD executes delay work once and body BRC+1 times, preserving unrelated state");
			expect(m_rptbd.observed.acks == unsigned(interrupt) && m_rptbd.observed.stack_reads == unsigned(interrupt) && m_rptbd.observed.stack_writes == unsigned(interrupt), "RPTBD balances one optional interrupt frame and acknowledges its source once");
			{ auto const disable = machine().disable_side_effects(); for (unsigned i = 0; i < iterations; ++i) expect(m_cpu->space(AS_DATA).read_word(0x6000 + i) == rptbd_source(variant, i), "RPTBD final copy matches independent source oracle"); }
			if (!m_rptbd.replay)
			{
				m_rptbd.replay = true; m_rptbd.tick = checkpoint; m_rptbd.observed = m_rptbd.snapshot;
				m_rptbd.return_pc = m_rptbd.saved_return_pc;
				m_rptbd.saved.clear(); m_rptbd.saved.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_rptbd.saved) == STATERR_NONE, "Restore RPTBD without fetching retained words or duplicating delay effects");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 255330; m_check_timer->enable(true); return true;
			}
			m_rptbd.read_tap.remove(); m_rptbd.write_tap.remove(); m_rptbd.fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (interrupt ? 1279U : 511U)) { start_rptbd(variant + 1); return true; }
			if (interrupt)
			{
				osd_printf_info("TMS320C54x delayed block repeat interrupts: PASS cases=768 restore=768 counts=0,1,2,7 delay_layouts=4 pages=2 sources=IRQ,NMI arrivals=delay,first_write snapshots=6 delay_effects=once ordered_copies=1 balanced_frames=1 retained_words=1 physical_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated delayed block repeat interrupts complete");
			}
			osd_printf_info("TMS320C54x delayed block repeat: PASS cases=512 restore=512 counts=0,1,2,7 delay_layouts=4 patterns=4 pages=2 snapshots=4 delay_effects=once ordered_copies=1 retained_words=1 interrupt_composition=unvalidated physical_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated delayed block repeat complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255330)
	{
		if (m_rptbd.variant >= 512) { m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); m_phase = 255331; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255320; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255331) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255320; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 255340)
	{
		unsigned const variant = m_rptbd.variant, kind = variant % 7, tick = ++m_rptbd.tick;
		unsigned const guard = kind == 5 ? 8 : kind == 4 ? 6 : 5, checkpoint = guard - 1 + variant / 14;
		if (tick == checkpoint && !m_rptbd.replay)
		{
			m_rptbd.snapshot = m_rptbd.observed; m_rptbd.saved.str({}); m_rptbd.saved.clear();
			expect(machine().save().write_stream(m_rptbd.saved) == STATERR_NONE, "Save RPTBD exclusion before or after the guard");
		}
		if (tick == 16)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->pipeline_cycles() == guard && !m_rptbd.observed.reads && !m_rptbd.observed.writes &&
				m_cpu->state_int(tms320c54x_device::STATE_BRC) == 2 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x5000 && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x6000 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5aa5,
				"RPTBD exclusions stop before bus effects, pointer updates, BRC writes or frame allocation");
			if (!m_rptbd.replay)
			{
				m_rptbd.replay = true; m_rptbd.tick = checkpoint; m_rptbd.observed = m_rptbd.snapshot;
				m_rptbd.saved.clear(); m_rptbd.saved.seekg(0); m_check_timer->adjust(attotime::never); expect(machine().save().read_stream(m_rptbd.saved) == STATERR_NONE, "Restore RPTBD exclusion at its exact retained guard");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 255350; m_check_timer->enable(true); return true;
			}
			m_rptbd.read_tap.remove(); m_rptbd.write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 27) { start_rptbd_edge(variant + 1); return true; }
			osd_printf_info("TMS320C54x delayed block repeat exclusions: PASS cases=28 restore=28 forms=STM_BRC,STLM_BRC,POPM_BRC,branch,spill,nested_body,nested_delay pages=2 snapshots=2 data_reads=0 data_writes=0 pointer_updates=0 counters_preserved=1 silicon_trap_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated delayed block repeat exclusions complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255350) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255340; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

void tms320c54x_test_state::acknowledge_rptbd_interrupt(int line)
{
		if (m_phase == 255320)
		{
			unsigned const variant = m_rptbd.variant, layout = rptbd_layout(variant);
			expect(variant >= 512 && line == rptbd_irq_line(variant) && !m_rptbd.observed.acks && m_cpu->pipeline_cycles() >= 9 &&
				m_rptbd.observed.delay_reads == unsigned(layout == 2) && m_rptbd.observed.delay_writes == unsigned(layout == 1), "RPTBD acknowledges its pending source only after complete delay work");
			++m_rptbd.observed.acks; m_cpu->set_input_line(line, CLEAR_LINE);
		}
}

} // namespace c54x_test
