// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

void tms320c54x_test_state::start_cala_rtn_case(unsigned index)
{
	m_cpu->reset();
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x0800, index ? 0xf5e3 : 0xf4e3); // CALA A/B
	program.write_word(0x0801, 0xf4e1);
	program.write_word(0x0900, 0xf4e1); // Inspect the paused callee before RETF.
	program.write_word(0x0901, 0xf49b);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_A, 0x0900}, {cpu_regs::STATE_B, 0x0900}, {cpu_regs::STATE_SP, 0x0400}, {STATE_GENPC, 0x0800},
	});
	m_phase = 87000 + index * 2;
	m_check_timer->adjust(attotime::from_usec(100));
}









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

void tms320c54x_test_state::start_conditional_call(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const tc_call = profile_is("tc-call");
	bool const returning = profile_is("conditional-ret");
	bool const signed_call = profile_is("signed-call"), all_conditions = returning || signed_call;
	unsigned const patterns = all_conditions ? 8 : 4, condition = all_conditions ? (variant / 32) % 6 : 3;
	u64 const value = std::array<u64, 8>{0, 1, 0x8000000000ULL, 0xffffffffffULL, 0x7fffffffffULL, 0x0100000000ULL, 0x0080000000ULL, 0x00ffffffffULL}[variant % patterns];
	s64 const signed_value = value & 0x8000000000ULL ? s64(value) - 0x10000000000LL : s64(value);
	bool const b = (variant / patterns) % 2;
	bool const taken = tc_call ? bool((variant / 2) & 1) == bool(variant & 1) : std::array<bool, 6>{signed_value >= 0, signed_value < 0, signed_value != 0, signed_value == 0, signed_value > 0, signed_value <= 0}[condition];
	u32 const base = (variant / (patterns * 2)) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) { program.write_word(base + 0x600 + i, 0xf495); program.write_word(base + 0x700 + i, 0xf495); }
	program.write_word(base + 0x600, returning ? 0xfc42 + condition + (b ? 8 : 0) : signed_call ? 0xf942 + condition + (b ? 8 : 0) : b ? 0xf94d : 0xf945); program.write_word(base + 0x601, returning ? 0xf4e1 : 0x700); program.write_word(base + 0x602, 0xf4e1); program.write_word(base + 0x701, 0xfc00);
	if (tc_call) program.write_word(base + 0x600, variant & 1 ? 0xf930 : 0xf920);
	if (returning) { program.write_word(base + 0x700, 0xf4e1); m_cpu->space(AS_DATA).write_word(0x1000, 0x700); }
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_ST0, 0xa608}, {cpu_regs::STATE_ST1, 0x800},
	});
	if (tc_call) m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608 | (((variant / 2) & 1) << 12));
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0}, {cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_SP, 0x1000},
		{cpu_regs::STATE_A, b ? 0x1234 : value}, {cpu_regs::STATE_B, b ? value : 0x2345},
	});
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = m_fast_stack_reads = 0;
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "cc_taken_frame",
		[this, taken, returning](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!returning && taken && !m_pipeline_store_writes && at == 0xfff && word == 0x602 && m_cpu->pipeline_cycles() == 7, "Only taken CC writes a stack frame; RC never writes data"); ++m_pipeline_store_writes; });
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "cc_return_frame",
		[this, taken, returning](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(taken && !m_pipeline_operand_reads && at == (returning ? 0x1000 : 0xfff) && word == (returning ? 0x700 : 0x602) && (!returning || m_cpu->pipeline_cycles() == 5), "RC/RET consumes one frame only on the taken path"); ++m_pipeline_operand_reads; });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x700, base + 0x700, "cc_target",
		[this, taken, returning](offs_t, u16 &, u16) { if (machine().side_effects_disabled()) return; expect(taken && !m_pipeline_extensions_reads && m_cpu->pipeline_cycles() == (returning ? 7 : 8), "Conditional target Fetch follows owned stack publication or sampling"); ++m_pipeline_extensions_reads; });
	m_operand_data_tap = program.install_read_tap(base + (returning ? 0x900 : 0x601), base + (returning ? 0x900 : 0x601), "cc_literal",
		[this](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!m_fast_stack_reads && word == 0x700, "CC retains its target literal once"); ++m_fast_stack_reads; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 253810;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::conditional_register_fixture tms320c54x_test_state::conditional_register_case(unsigned index)
{
	unsigned const condition = index % 12, pattern = (index / 12) % 6, x = (index / 72) % 16;
	u64 const tested = std::array<u64, 6>{0, 1, 0xffffffffffULL, 0x7fffffff, 0xff80000000ULL, 0x0100000000ULL}[pattern];
	bool const negative = pattern == 2 || pattern == 4;
	bool const predicates[] = {!negative, negative, pattern != 0, pattern == 0, !negative && pattern != 0, negative || pattern == 0};
	u16 const address = 0x1403 + (x & 3) * 16;
	u16 const next = (x >> 2) == 0 ? address : (x >> 2) == 1 ? address - 1 : (x >> 2) == 2 ? address + 1 : address - 2;
	return {bool((index / 4608) % 2), predicates[condition % 6], x,
		std::array<u16, 4>{0, 0xffff, 0x1234, 0x8000}[(index / 1152) % 4], address, next,
		condition < 6 ? tested : 0x23456789ULL, condition < 6 ? 0x23456789ULL : tested};
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













bool tms320c54x_test_state::check_conditional_call_results()
{
	if (m_phase == 253810)
	{
		bool const tc_call = profile_is("tc-call");
		bool const returning = profile_is("conditional-ret");
		bool const signed_call = profile_is("signed-call"), all_conditions = returning || signed_call;
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, checkpoint = (returning ? std::array<unsigned, 4>{3, 4, 5, 7} : std::array<unsigned, 4>{5, 6, 7, 9})[variant / (all_conditions ? 192 : 16)];
		unsigned const patterns = all_conditions ? 8 : 4, condition = all_conditions ? (variant / 32) % 6 : 3;
		u64 const value = std::array<u64, 8>{0, 1, 0x8000000000ULL, 0xffffffffffULL, 0x7fffffffffULL, 0x0100000000ULL, 0x0080000000ULL, 0x00ffffffffULL}[variant % patterns];
		s64 const signed_value = value & 0x8000000000ULL ? s64(value) - 0x10000000000LL : s64(value);
		bool const b = (variant / patterns) % 2;
		bool const taken = tc_call ? bool((variant / 2) & 1) == bool(variant & 1) : std::array<bool, 6>{signed_value >= 0, signed_value < 0, signed_value != 0, signed_value == 0, signed_value > 0, signed_value <= 0}[condition];
		u64 const changed = all_conditions ? (taken ? std::array<u64, 6>{0x8000000000ULL, 0, 0, 1, 0, 1} : std::array<u64, 6>{0, 0x8000000000ULL, 1, 0, 1, 0})[condition] : u64(taken);
		u32 const base = (variant / (patterns * 2)) % 2 ? 0x20000 : 0;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Only admitted CC/RC predicates execute in the staged frontend");
		if (!taken) expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000, "Untaken CC leaves SP unchanged throughout the transaction");
		if (returning) expect(bool(m_cpu->state_int(tms320c54x_device::STATE_IDLE)) == (tick >= (taken ? 11 : 9)), "RC preserves Class 32's five-cycle taken and three-cycle untaken cadence");
		if (tick == 5) { auto const disable = machine().disable_side_effects(); if (returning) m_cpu->space(AS_DATA).write_word(0x1000, 0xdead); else m_cpu->space(AS_PROGRAM).write_word(base + 0x601, 0xdead); }
		if (tick == (returning ? 5 : 6)) m_cpu->set_state_int(b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A, changed);
		if (tc_call && tick == 6) m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa608 | ((((variant / 2) & 1) ^ 1) << 12));
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_pipeline_extensions_reads, &m_fast_stack_reads},
			"Save CC around predicate, held clock and conditional publication");
		if (tick == 32)
		{
			expect(m_cpu->state_int(b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == changed && m_cpu->state_int(b ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == (b ? 0x1234 : 0x2345), "CC/RC preserves the changed predicate source and unselected accumulator");
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == base + (returning ? taken ? 0x701 : 0x602 : 0x603) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 + unsigned(returning && taken) && m_pipeline_operand_reads == unsigned(taken) && m_pipeline_store_writes == unsigned(!returning && taken) && m_pipeline_extensions_reads == unsigned(taken) && m_fast_stack_reads == unsigned(!returning) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0xa608 | (tc_call ? ((((variant / 2) & 1) ^ 1) << 12) : 0)) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800, "CC/RC retains its decision and target; untaken paths never touch SP or stack memory");
			if (replay_pipeline_at(253820, checkpoint,
				"Replay conditional allocation and call/return without duplicate accesses")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_operand_data_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (all_conditions ? 767 : 63)) { start_conditional_call(variant + 1); return true; }
			if (returning)
			{
				osd_printf_info("TMS320C54x staged conditional return: PASS cases=768 restore=768 conditions=AGEQ,ALT,ANEQ,AEQ,AGT,ALEQ,BGEQ,BLT,BNEQ,BEQ,BGT,BLEQ patterns=8 pages=2 snapshots=4 taken_reads=1 data_writes=0 untaken_stack_accesses=0 predicate_retained=1 stack_target_retained=1 delayed_acceptance=0 silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated conditional return complete");
			}
			if (tc_call)
			{
				osd_printf_info("TMS320C54x TC conditional calls: PASS cases=64 restore=64 predicates=TC,NTC initial_tc=2 pages=2 snapshots=4 literal_fetches=1 taken_frame=1 balanced_return=1 untaken_stack_accesses=0 predicate_retained=1 target_retained=1 delayed_acceptance=0 silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated TC conditional calls complete");
			}
			if (signed_call)
			{
				osd_printf_info("TMS320C54x signed conditional calls: PASS cases=768 restore=768 predicates=AGEQ,ALT,ANEQ,AEQ,AGT,ALEQ,BGEQ,BLT,BNEQ,BEQ,BGT,BLEQ patterns=8 pages=2 snapshots=4 literal_fetches=1 taken_frame=1 balanced_return=1 untaken_stack_accesses=0 predicate_retained=1 target_retained=1 silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated signed conditional calls complete");
			}
			osd_printf_info("TMS320C54x staged conditional call: PASS cases=64 restore=64 conditions=AEQ,BEQ patterns=4 pages=2 literal_fetches=1 taken_frame=1 untaken_stack_accesses=0 predicate_retained=1 delayed_acceptance=0 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated conditional call complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253820) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253810; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}




bool tms320c54x_test_state::check_conditional_register_results()
{
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 500000 && m_phase < 518432)
	{
		unsigned const index = m_phase - 500000;
		auto const f = conditional_register_case(index);
		if (!m_rom4_codec_replays && m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x601)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save conditional register store after its read/write and address update");
			m_rom4_codec_replays = 1;
		}
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Conditional register store remains admitted");
		if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE)) { m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
		auto const disable = machine().disable_side_effects();
		expect(m_rom4_codec_replays == 1 && m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 &&
			data.read_word(f.address) == (f.take ? f.source : 0xa55a) && m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x602 &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == f.a && m_cpu->state_int(tms320c54x_device::STATE_B) == f.b &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == (f.brc ? 0xbeef : f.source) &&
			m_cpu->state_int(tms320c54x_device::STATE_BRC) == (f.brc ? f.source : 0x55aa) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x1e05 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800,
			"Conditional register store preserves source, predicates and status while completing one bus pair");
		for (unsigned ar = 2; ar < 6; ++ar)
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + ar) == (ar == 2 + (f.x & 3) ? f.next : 0x1403 + (ar - 2) * 16), "Conditional register store updates only its selected Xmem address, even when false");
		if (!m_rom4_codec_replay)
		{
			expect(restore_snapshot(machine(), *m_check_timer, m_saved_pipeline) == STATERR_NONE, "Restore conditional register store without duplicating a completed bus transaction");
			m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1];
			m_phase = 500000 + index; m_rom4_codec_replay = true;
			m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
		}
		if (index < 18431) { start_conditional_register_store(index + 1); return true; }
		osd_printf_info("TMS320C54x atomic conditional register stores: PASS cases=18432 restore=18432 forms=STRCD,SRCCD predicates=12 patterns=6 xmem=16 source_values=4 snapshots=2 false_read_writeback=1 circular_update=1 silicon_phase_claim=0\n");
		throw emu_fatalerror(0, "TMS320C54x isolated conditional register stores complete");
	}
	return false;
}


} // namespace c54x_test
