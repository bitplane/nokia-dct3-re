// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"
#include "tms320c54x_test_word_examples.h"

namespace c54x_test {

void tms320c54x_test_state::start_repeat_endpoint_case(unsigned index)
{
	m_cpu->reset();
	auto &program = m_cpu->space(AS_PROGRAM);
	auto &data = m_cpu->space(AS_DATA);
	u16 const body = index % 3 == 0 ? 0x0802 : index % 3 == 1 ? 0xfffd : 0xfffe;
	bool const long_count = (index / 3) & 1, nmi = index >= 6;
	u16 const start = body - (long_count ? 2 : 1);
	program.write_word(0x010000 | start, long_count ? 0xf070 : 0xec02);
	if (long_count) program.write_word(0x010000 | u16(start + 1), 2);
	program.write_word(0x010000 | body, 0x7312); // MVMD AR2,dmad
	program.write_word(0x010000 | u16(body + 1), 0x0600);
	program.write_word(0x010000 | u16(body + 2), 0xf4e1);
	program.write_word(nmi ? 0x010004 : 0x010040, 0xf4e1); // Stop at accepted vector.
	for (unsigned i = 0; i < 4; ++i) data.write_word(0x0600 + i, 0xdead);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_AR2, 0x1234}, {cpu_regs::STATE_SP, 0x1000}, {cpu_regs::STATE_ST1, 0},
		{cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_IMR, 1}, {STATE_GENPC, 0x010000 | start},
	});
	m_repeat_boundary_requests = 0;
	m_repeat_boundary_tap.remove();
	m_repeat_boundary_tap = data.install_write_tap(0x0601, 0x0601, "repeat_final_request",
		[this, nmi](offs_t, u16 &, u16) {
			if (!machine().side_effects_disabled() && !m_repeat_boundary_requests++)
				m_cpu->set_input_line(nmi ? INPUT_LINE_NMI : 0, ASSERT_LINE);
		});
	m_phase = 83000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_loop_xc_case(unsigned index)
{
	m_cpu->reset();
	auto &program = m_cpu->space(AS_PROGRAM);
	bool const long_body = index & 1, nmi = index >= 2;
	program.write_word(0x0800, 0xf072); // RPTB end
	program.write_word(0x0801, long_body ? 0x0805 : 0x0804);
	program.write_word(0x0802, 0xe809); // LD #9,A: the next loop's first word.
	program.write_word(0x0803, long_body ? 0xff4d : 0xfd4d); // XC 2/1,BEQ
	program.write_word(0x0804, long_body ? 0xf020 : 0xe801); // LD #1,A
	if (long_body) program.write_word(0x0805, 1);
	program.write_word(long_body ? 0x0806 : 0x0805, 0xf4e1);
	u16 const vector = nmi ? 0x0004 : 0x0048;
	program.write_word(vector, 0x0083); // Passive ISR observation through *AR3.
	program.write_word(vector + 1, 0xf4e1);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_BRC, 1}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_AR3, 0x0061}, {cpu_regs::STATE_SP, 0x1000},
		{cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_IMR, 4}, {cpu_regs::STATE_PC, 0x0800},
	});
	m_irq_accumulator = 0xdead;
	m_xc_irq_fetches = 0;
	m_xc_irq_tap.remove();
	m_xc_irq_tap = program.install_read_tap(0x0804, 0x0804, "xc_loop_pending_irq",
		[this, nmi](offs_t, u16 &, u16) {
			if (!machine().side_effects_disabled() && !m_xc_irq_fetches++)
				m_cpu->set_input_line(nmi ? INPUT_LINE_NMI : 2, ASSERT_LINE);
		});
	m_phase = 85000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_nested_repeat_case(unsigned index)
{
	m_cpu->reset();
	auto &program = m_cpu->space(AS_PROGRAM);
	bool const long_count = index >= 2;
	u16 const count = index & 1 ? 2 : 0;
	u16 const body = long_count ? 0x0804 : 0x0803;
	program.write_word(0x0800, 0xf072); // RPTB body
	program.write_word(0x0801, body);
	program.write_word(0x0802, long_count ? 0xf070 : 0xec00 | count);
	if (long_count) program.write_word(0x0803, count);
	program.write_word(body, 0x6d91); // MAR *AR1+
	program.write_word(body + 1, 0xf4e1); // IDLE1
	set_registers(*m_cpu, {
		{cpu_regs::STATE_BRC, 3}, {cpu_regs::STATE_AR1, 0}, {cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_PC, 0x0800},
	});
	m_phase = 84000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_braf_control_case(unsigned index)
{
	m_cpu->reset();
	auto &program = m_cpu->space(AS_PROGRAM);
	auto &data = m_cpu->space(AS_DATA);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_BRC, 3}, {cpu_regs::STATE_AR1, 0}, {cpu_regs::STATE_SP, 0x1000}, {cpu_regs::STATE_ST1, 0},
	});
	if (index < 3)
	{
		program.write_word(0x0800, 0xf072); // RPTB end
		program.write_word(0x0801, index == 1 ? 0x0804 : 0x0803);
		program.write_word(0x0802, 0x6d91); // MAR *AR1+
		program.write_word(0x0803, index == 0 ? 0xf6bf : index == 1 ? 0x7707 : 0x8a07);
		program.write_word(0x0804, index == 1 ? 0 : 0xf4e1);
		program.write_word(0x0805, 0xf4e1);
		for (unsigned i = 0; i < 4; ++i) data.write_word(0x1000 + i, 0);
	}
	else
	{
		// Restore the architectural loop registers and BRAF without RPTB.
		data.write_word(0x001b, 0x0802); data.write_word(0x001c, 0x0803);
		program.write_word(0x0800, 0x7707); program.write_word(0x0801, 0x8000);
		program.write_word(0x0802, 0x6d91); program.write_word(0x0803, 0xf495);
		program.write_word(0x0804, 0xf4e1);
	}
	m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0800);
	m_phase = 82000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_repeated_writa(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const standard_width = profile_is("rpt-writa16");
	auto const f = repeated_writa_case(variant, standard_width); m_cpu->set_extended_program(!standard_width);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.page + 0x9000 + i, 0xf495);
	program.write_word(f.page + 0x9000, f.setup == 0 ? 0xec00 | f.count : f.setup == 1 ? 0xf070 : f.setup == 2 ? 0x4702 : 0x47f8);
	if (f.words == 2) program.write_word(f.page + 0x9001, f.setup == 1 ? f.count : f.count_address);
	program.write_word(f.page + 0x9000 + f.words, 0x7f00 | (f.form < 2 ? 1 : (f.form == 2 ? 0x80 : f.form == 3 ? 0x88 : 0x90) | f.reg));
	program.write_word(f.page + 0x9001 + f.words, 0xf4e1);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_ST0, 0xa824},
		{cpu_regs::STATE_ST1, 0x900 | (f.form == 1 ? 0x4000 : 0)}, {cpu_regs::STATE_PMST, f.mapped ? 0x20 : 0},
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
		{cpu_regs::STATE_A, 0xff00000000ULL | f.target}, {cpu_regs::STATE_B, 0x81abcdef01ULL},
		{cpu_regs::STATE_SP, 0x1200}, {cpu_regs::STATE_T, 0x5aa5},
	});
	for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, f.form >= 2 && ar == f.reg ? f.source : 0x4000 + ar);
	if (f.setup == 4) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ((f.reg + 1) % 8), f.count);
	else if (f.setup >= 2) data.write_word(f.count_address, f.count);
	data.write_word(f.source, repeated_writa_word(f.reg, 0));
	m_pipeline_operand_reads = m_pipeline_store_writes = m_port_reads = m_pipeline_register_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "repeat_writa_reads",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			if (f.setup >= 2 && at == f.count_address) { expect(!m_port_reads && word == f.count, "WRITA memory-repeat setup samples its count once"); ++m_port_reads; return; }
			unsigned const n = m_pipeline_operand_reads;
			expect(n <= f.count && n == m_pipeline_store_writes && at == u16(f.source + f.direction * int(n)) && word == repeated_writa_word(f.reg, n) && m_cpu->pipeline_cycles() == (n ? f.write + n : f.read), "Repeated WRITA reads each fresh source once, including its retained first operand"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "repeat_writa_no_data_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "WRITA writes program memory, never data memory"); });
	m_pipeline_fetch_tap = program.install_write_tap(0, 0x7fffff, "repeat_writa_program_write",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			unsigned const n = m_pipeline_store_writes;
			expect(n <= f.count && m_pipeline_operand_reads == n + 1 && at == repeated_writa_destination(f, n) && word == repeated_writa_word(f.reg, n) && m_cpu->pipeline_cycles() == f.write + n, "WRITA advances retained logical PAR and maps each one-clock stream write correctly"); ++m_pipeline_store_writes;
			if (n < f.count) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(u16(f.source + f.direction * int(n + 1)), repeated_writa_word(f.reg, n + 1)); }
		});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = repeated_writa_phase;
	m_cpu->set_state_int(STATE_GENPC, f.page + 0x9000); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::delayed_condition_return_fixture tms320c54x_test_state::delayed_condition_return_case(unsigned v)
{
	delayed_condition_return_fixture f{};
	f.condition = v % 14; unsigned const pattern = (v / 14) % 8;
	f.layout = (v / 112) % 2; f.sp = 0x1000 + (v / 224) % 2; f.base = (v / 448) % 2 ? 0x20000 : 0;
	f.checkpoint = std::array<unsigned, 5>{3, 4, 5, 6, 8}[v / 896];
	u64 const raw = std::array<u64, 8>{0, 1, 0xffffffffffULL, 0x7fffffff, 0x80000000, 0xff80000000ULL, 0x7fffffffffULL, 0x8000000000ULL}[pattern];
	f.a = f.condition < 6 ? raw : 0x12345678; f.b = f.condition >= 6 && f.condition < 12 ? raw : 0x23456789;
	f.opcode = f.condition >= 12 ? (f.condition == 13 ? 0xfe30 : 0xfe20) : 0xfe42 + f.condition % 6 + (f.condition >= 6 ? 8 : 0);
	f.st0 = 0xa608 | (pattern & 1 ? 0x1000 : 0);
	if (f.condition >= 12) f.taken = bool(pattern & 1) == (f.condition == 13);
	else
	{
		s64 const value = s64(raw << 24) >> 24;
		switch (f.condition % 6) { case 0: f.taken = value >= 0; break; case 1: f.taken = value < 0; break; case 2: f.taken = value != 0; break; case 3: f.taken = value == 0; break; case 4: f.taken = value > 0; break; default: f.taken = value <= 0; break; }
	}
	f.final_a = f.layout ? f.a : 0; f.final_b = f.layout || f.condition >= 12 ? f.b : 0;
	f.final_st0 = !f.layout && f.condition >= 12 ? f.st0 ^ 0x1000 : f.st0;
	return f;
}

void tms320c54x_test_state::start_rptz(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const target = variant % 2, count = std::array<unsigned, 4>{0, 1, 3, 17}[(variant / 2) % 4];
	u32 const base = (variant / 128) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 12; ++i) program.write_word(base + 0x9000 + i, 0xf495);
	program.write_word(base + 0x9000, 0xf071 | (target << 8)); program.write_word(base + 0x9001, count);
	program.write_word(base + 0x9002, 0x7693); program.write_word(base + 0x9003, 0x2468); program.write_word(base + 0x9004, 0xf4e1);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, 0xa024 | (((variant / 8) % 4) << 11)},
		{cpu_regs::STATE_ST1, 0x800 | (((variant / 32) % 4) << 8)}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
		{cpu_regs::STATE_A, 0xff12345678ULL}, {cpu_regs::STATE_B, 0x81abcdef01ULL}, {cpu_regs::STATE_AR3, 0x1200},
	});
	m_pipeline_extensions_reads = m_pipeline_store_writes = 0;
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "rptz_writes",
		[this, count](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(m_pipeline_store_writes <= count && at == 0x1200 + m_pipeline_store_writes && word == 0x2468, "RPTZ issues exactly count-plus-one ordered real stores"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x9001, base + 0x9001, "rptz_literal",
		[this, count](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(word == count && !m_pipeline_extensions_reads, "RPTZ retains its count literal once"); ++m_pipeline_extensions_reads; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = rptz_phase;
	m_cpu->set_state_int(STATE_GENPC, base + 0x9000); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_repeat_nop(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const long_form = variant % 2;
	unsigned const count = std::array<unsigned, 5>{0, 1, 3, 32, 255}[(variant / 2) % 5], interrupt = (variant / 10) % 3;
	u32 const base = (variant / 30) % 2 ? 0x20000 : 0, body = base + (long_form ? 0x602 : 0x601), continuation = body + 1;
	unsigned const last = (long_form ? 8 : 7) + count;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, long_form ? 0xf070 : 0xec00 | count);
	if (long_form) program.write_word(base + 0x601, count);
	program.write_word(body, 0xf495); program.write_word(continuation, 0xe855); program.write_word(continuation + 1, 0xf4e1);
	if (interrupt)
	{
		// With OVLY clear, vectors inherit XPC even for NMI's low slot 4.
		u32 const vector = base + (interrupt == 2 ? 4 : 0x40);
		program.write_word(vector, 0x4a1e); program.write_word(vector + 1, 0xf495); program.write_word(vector + 2, 0xf4e5);
	}
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, 0x608}, {cpu_regs::STATE_ST1, interrupt == 1 ? 0 : 0x800}, {cpu_regs::STATE_SP, 0x1000},
	});
	set_registers(*m_cpu, {
		{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0x1234}, {cpu_regs::STATE_IFR, 0},
		{cpu_regs::STATE_IMR, interrupt == 1 ? 1 : 0},
	});
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = m_vector_acks = 0;
	m_pipeline_operand_tap = program.install_read_tap(body, body, "repeat_nop_body",
		[this](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(word == 0xf495 && !m_pipeline_operand_reads, "Repeated NOP body is fetched once, not reread from poisoned program memory"); ++m_pipeline_operand_reads; });
	m_pipeline_fetch_tap = program.install_read_tap(continuation, continuation, "repeat_nop_continuation",
		[this, last](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(word == (m_pipeline_extensions_reads ? 0xe866 : 0xe855) && (!m_pipeline_extensions_reads || m_cpu->pipeline_cycles() > last),
				"Repeat discards speculative continuation and refetches after its final NOP"); ++m_pipeline_extensions_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "repeat_nop_no_data_write",
		[this, interrupt, last](offs_t at, u16 &, u16) { if (machine().side_effects_disabled()) return;
			expect(interrupt && m_cpu->pipeline_cycles() > last && at >= 0xffe && at <= 0xfff && m_pipeline_store_writes < 2,
				"Repeated NOP has no data write; only a later interrupt can write its stack"); ++m_pipeline_store_writes; });
	m_phase = 252750; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_rptb(unsigned variant)
{
	bool const irq = variant >= 6144;
	unsigned const flags = irq ? 0 : (variant >> 7) & 15;
	u32 const base = (irq ? (variant - 6144) & 2 : variant & 64) ? 0x20000 : 0;
	start_pipeline_swap_body((flags << 4) | (base ? 256 : 0));
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove();
	static constexpr u16 words[] = {0xf072, 0x0606, 0x1386, 0xf678, 0xf3e8, 0xf1a0, 0x8196, 0xf4e1};
	for (unsigned i = 0; i < 24; ++i) m_cpu->space(AS_PROGRAM).write_word(base + 0x600 + i, i < std::size(words) ? words[i] : 0xf495);
	for (unsigned i = 0; i <= rptb_count(variant); ++i) m_cpu->space(AS_DATA).write_word(0x5000 + i, rptb_source(variant, i));
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC, rptb_count(variant));
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	if (irq) m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, irq ? 4 : 0);
	u16 const vector = (variant - 6144) & 1 ? 0x0004 : 0x0048;
	m_cpu->space(AS_PROGRAM).write_word(base | vector, 0xf495); m_cpu->space(AS_PROGRAM).write_word((base | vector) + 1, 0xf4eb);
	m_pipeline_extensions_reads = 0;
	m_vector_prefetch_tap = m_cpu->space(AS_PROGRAM).install_read_tap(base | vector, base | vector, "rptb_vector",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_extensions_reads; });
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0x5000, 0x50ff, "rptb_body_read",
		[this, variant, irq](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			unsigned const index = m_pipeline_operand_reads;
			expect(index <= rptb_count(variant) && at == 0x5000 + index && word == rptb_source(variant, index) &&
				(irq || m_cpu->pipeline_cycles() == 9 + index * 5), "RPTB reads each successive source once with zero-overhead body spacing"); ++m_pipeline_operand_reads;
			if (irq && index == 1) m_cpu->set_input_line((variant - 6144) & 1 ? INPUT_LINE_NMI : 2, ASSERT_LINE); });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0x5000, 0x50ff, "rptb_body_write",
		[this, variant, irq](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			unsigned const index = m_pipeline_store_writes; u16 const source = rptb_source(variant, index);
			expect(index <= rptb_count(variant) && at == 0x5000 + index && word == u16((source << 8) | (source >> 8)) &&
				(irq || m_cpu->pipeline_cycles() == 14 + index * 5), "RPTB writes each transformed word once through interrupt and restore"); ++m_pipeline_store_writes; });
	m_phase = 253350; m_cpu->resume(SUSPEND_REASON_DISABLE);
}

unsigned tms320c54x_test_state::rptbd_seed(unsigned v)
{ return v < 512 ? v : (v - 512) % 16 + (((v - 512) / 16) % 2) * 64; }

unsigned tms320c54x_test_state::rptbd_count(unsigned v)
{ return std::array<unsigned, 4>{0, 1, 2, 7}[rptbd_seed(v) % 4]; }

unsigned tms320c54x_test_state::rptbd_layout(unsigned v)
{ return (rptbd_seed(v) / 4) % 4; }

u32 tms320c54x_test_state::rptbd_base(unsigned v)
{ return (rptbd_seed(v) / 64) % 2 ? 0x20000 : 0; }

u16 tms320c54x_test_state::rptbd_source(unsigned v, unsigned i)
{ return std::array<u16, 4>{0, 1, 0x8000, 0xffff}[(rptbd_seed(v) / 16) % 4] ^ u16(i * 0x1357); }

tms320c54x_test_state::basic_repeat_fixture tms320c54x_test_state::basic_repeat_case(unsigned v)
{
	unsigned const edge = v >= 10368 ? v - 10368 : 8;
	if (edge < 8) v = ((edge / 2) % 2) * 576 + (edge / 4) * 3456;
	basic_repeat_fixture f{};
	f.form = v % 36; v /= 36; f.count = std::array<unsigned, 4>{0, 1, 7, 255}[v % 4]; v /= 4;
	if (edge < 8) f.count = edge % 2 ? 0xffff : 0x8000;
	f.compiler = v % 2; v /= 2; f.compatibility = v % 2; v /= 2;
	f.base = v % 2 ? 0x20000 : 0; v /= 2; f.irq = v % 3; v /= 3;
	f.checkpoint = std::array<unsigned, 3>{3, 6, 12}[v];
	f.absolute = f.form >= 34;
	f.mode = f.form < 2 ? (f.form ? 0x7f : 0) : f.form < 26 ? 0x80 + f.form - 2 : f.form < 34 ? 0x10 + f.form - 26 : 0xf8;
	f.selected = f.mode & 7;
	if (f.compatibility && !f.selected) f.selected = 5;
	f.modification = f.form >= 2 && f.form < 26 ? (f.mode & 0x78) >> 3 : 0;
	f.st0 = 0xa600 | (f.form >= 26 && f.form < 34 ? 0 : 8);
	f.st1 = (f.compiler ? 0x4000 : 0) | (f.compatibility ? 0x20 : 0) | (f.irq ? 0 : 0x800);
	f.address = f.absolute ? (f.form == 35 ? 0x15 : 0x5000) : f.form >= 2 && f.form < 26 ? 0x5000 :
		f.compiler ? 0x3000 + f.mode : ((f.st0 & 0x1ff) << 7) | f.mode;
	f.body = f.base + (f.absolute ? 0x602 : 0x601);
	return f;
}

void tms320c54x_test_state::start_basic_repeat(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = basic_repeat_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 24; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, 0x4700 | f.mode);
	if (f.absolute) program.write_word(f.base + 0x601, f.address);
	program.write_word(f.base + 0x60f, 0xf4e1);
	u32 const vector = f.base + (f.irq == 2 ? 4 : 0x48);
	program.write_word(vector, 0xf495); program.write_word(vector + 1, 0xf4eb);
	m_cpu->set_pipeline_frontend(false); m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ST0, f.st0},
		{cpu_regs::STATE_ST1, f.st1}, {cpu_regs::STATE_SP, 0x3000}, {cpu_regs::STATE_A, 0x123456789aULL},
		{cpu_regs::STATE_B, 0xfedcba9876ULL}, {cpu_regs::STATE_RC, 0}, {cpu_regs::STATE_BRC, 13},
		{cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, f.irq == 1 ? 4 : 0},
	});
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, f.address == 0x10 + i ? f.count : 0x5000);
	if (f.address >= 0x60) data.write_word(f.address, f.count);
	m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_observed_instructions.clear(); m_saved_observed_instructions.clear(); m_rptbd_counts = {}; m_rptbd_return = 0; m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_fetch_tap = program.install_read_tap(f.body, f.body, "basic_repeat_body", [this](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(word == 0xf495 && !m_rptbd_counts.fetches++, "Memory-count repeat fetches its body once"); });
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "basic_repeat_reads", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at == 0x2fff) { expect(f.irq && !m_rptbd_counts.stack_reads++ && word == m_rptbd_return, "Memory repeat interrupt consumes its retained continuation"); return; }
		expect(at == f.address && word == f.count && !m_rptbd_counts.reads++ && m_cpu->pipeline_cycles() == (f.absolute ? 6 : 5), "Memory repeat samples the count exactly once through the decoded address"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "basic_repeat_writes", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		expect(f.irq && at == 0x2fff && !m_rptbd_counts.stack_writes++ && word >= u16(f.body + 1) && word <= 0x60f, "Memory repeat writes only its optional post-body interrupt frame"); m_rptbd_return = word; });
	m_phase = 310440; m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::rptbd_square_fixture tms320c54x_test_state::rptbd_square_case(unsigned v) const
{
	rptbd_square_fixture f{};
	f.memory_square = profile_is("rptbd-squr");
	if (!f.memory_square) { f.operation = v % 2; v /= 2; }
	f.destination = v % 2; v /= 2; f.pattern = v % 8; v /= 8; f.flags = v % 16; v /= 16;
	unsigned const layouts = f.memory_square ? 5 : 4;
	f.layout = v % layouts; v /= layouts; f.form = v % 3; v /= 3; f.count = std::array<unsigned, 3>{0, 1, 7}[v % 3]; v /= 3;
	f.base = v % 2 ? 0x20000 : 0; v /= 2; f.snapshot = v;
	return f;
}

void tms320c54x_test_state::start_rptbd_square(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant; auto const f = rptbd_square_case(variant);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	program.write_word(f.base + 0x600, 0xf272); program.write_word(f.base + 0x601, 0x605);
	program.write_word(f.base + 0x602, f.opcode(0)); program.write_word(f.base + 0x603, f.opcode(1));
	program.write_word(f.base + 0x604, 0x8092); program.write_word(f.base + 0x605, 0x8193); program.write_word(f.base + 0x606, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, f.flags & 4 ? 2 : 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0()); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1());
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.result(0)[0]); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.result(0)[1]);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x5555); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000); m_cpu->set_state_int(tms320c54x_device::STATE_BRC, f.count);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x6000); m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x6100); m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x5000);
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	for (unsigned i = 0; i < f.operations(); ++i) data.write_word(f.address(i), f.memory(i));
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "rptbd_square_source", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; unsigned const i = m_pipeline_operand_reads++; expect(i < f.operations() && at == f.address(i) && word == f.memory(i) && m_cpu->pipeline_cycles() == 7 + f.slot(i), "RPTBD square delay samples each memory word once at Read"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "rptbd_square_body", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; bool const b = at >= 0x6100; unsigned &count = b ? m_pipeline_register_writes : m_pipeline_store_writes; expect(m_pipeline_operand_reads == f.operations() && count <= f.count && at == (b ? 0x6100 : 0x6000) + count && word == u16(f.result(f.operations())[b]), "RPTBD body sees completed square results on every iteration"); ++count; });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x603, "rptbd_square_words", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; unsigned const i = m_pipeline_extensions_reads++; expect(i < 3 && at == f.base + 0x601 + i && word == (i ? f.opcode(i - 1) : 0x605), "RPTBD owns one endpoint and two non-repeated delay words"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62178;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_rptb_edge(unsigned variant)
{
	start_pipeline_rptb(variant & 8 ? 64 : 0);
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_vector_prefetch_tap.remove();
	unsigned const kind = variant & 7; u32 const base = variant & 8 ? 0x20000 : 0;
	u16 const op = std::array<u16, 8>{0x6d96, 0xf020, 0xf272, 0xf073, 0x891a, 0xf020, 0x76f8, 0x8186}[kind];
	u16 const end = kind == 1 || kind == 2 || kind == 3 || kind == 6 ? 0x603 : 0x602;
	m_cpu->space(AS_PROGRAM).write_word(base + 0x600, 0xf072);
	m_cpu->space(AS_PROGRAM).write_word(base + 0x601, end);
	m_cpu->space(AS_PROGRAM).write_word(base + 0x602, op);
	m_cpu->space(AS_PROGRAM).write_word(base + 0x603, kind == 1 || kind == 5 ? 0x1234 : kind == 6 ? 0x5000 : kind == 3 ? 0x602 : kind == 2 ? 0x604 : 0xf4e1);
	m_cpu->space(AS_PROGRAM).write_word(base + 0x604, kind == 6 ? 0xbeef : 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC, 3);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR6, kind == 7 ? 0x1a : 0x5000);
	m_pipeline_extensions_reads = 0;
	m_pipeline_fetch_tap = m_cpu->space(AS_PROGRAM).install_read_tap(base + 0x603, base + 0x603, "rptb_edge_literal",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_extensions_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "rptb_edge_no_writes",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_store_writes; });
	m_phase = 253370; m_cpu->resume(SUSPEND_REASON_DISABLE);
}

unsigned tms320c54x_test_state::block_branch_count(unsigned variant)
{ return std::array<unsigned, 4>{0, 1, 2, 7}[variant % 4]; }

u16 tms320c54x_test_state::block_branch_source(unsigned variant, unsigned i)
{
	unsigned const pattern = (variant / 4) % 4;
	return pattern == 0 ? 0 : pattern == 1 ? 1 : pattern == 2 ? i & 1 : i == block_branch_count(variant) ? 0 : 1;
}

tms320c54x_test_state::block_exit_fixture tms320c54x_test_state::block_exit_case(unsigned v, bool delayed)
{
	unsigned const forms = delayed ? 13 : 3;
	unsigned const count = std::array<unsigned, 4>{0, 1, 2, 7}[v % 4], pattern = (v / 4) % 4, form = (v / 16) % forms;
	unsigned const exit = !form || !pattern ? 0 : pattern == 1 ? count + 1 : pattern == 2 ? count / 2 : count;
	bool const taken = exit <= count, endpoint_store = !((v / (16 * forms)) % 2);
	unsigned const body_writes = taken ? exit : count + 1;
	return {count, exit, taken ? exit + 1 : count + 1, body_writes,
		body_writes + unsigned(taken && endpoint_store), std::array<unsigned, 6>{3, 8, 9, 10, 12, 18}[v / ((delayed ? 128 : 64) * forms)], form,
		(v / (32 * forms)) % 2 ? 0x20000U : 0, u16((delayed ? 2 : 0) + (endpoint_store ? 0x606 : 0x609)), delayed ? form >= 7 : form == 2, taken, endpoint_store, delayed, delayed && (v / (64 * forms)) % 2};
}

void tms320c54x_test_state::start_block_exit(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = block_exit_case(variant, profile_is("rptb-exit-delay")); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 0x120; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, 0xf072); program.write_word(f.base + 0x601, f.end);
	program.write_word(f.base + 0x602, f.b ? 0x1192 : 0x1092);
	program.write_word(f.base + 0x603, f.delayed ? f.form ? 0xfa42 + ((f.form - 1) % 6) + (f.b ? 8 : 0) : 0xf273 : f.form ? f.b ? 0xf84d : 0xf845 : 0xf073);
	program.write_word(f.base + 0x604, 0x700);
	if (f.delayed)
	{
		program.write_word(f.base + 0x605 + unsigned(f.reverse_delay), f.b ? 0x1195 : 0x1095);
		program.write_word(f.base + 0x606 - unsigned(f.reverse_delay), f.b ? 0x8194 : 0x8094);
	}
	program.write_word(f.base + f.tail(), 0x7693); program.write_word(f.base + f.tail() + 1, 0x1111);
	program.write_word(f.base + f.end + 1, 0xf4e1);
	program.write_word(f.base + 0x700, 0x76f8); program.write_word(f.base + 0x701, 0x5200); program.write_word(f.base + 0x702, 0xcccc);
	program.write_word(f.base + 0x703, 0xf6bf);
	// SPRU131G 7.5.8.2: do not revisit the old endpoint until six
	// complete words after software BRAF clear. The branch adds more spacing.
	program.write_word(f.base + 0x70a, 0xf073); program.write_word(f.base + 0x70b, f.endpoint_store ? f.tail() : f.end);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_ST0, 0xa608}, {cpu_regs::STATE_ST1, 0x900}, {cpu_regs::STATE_IFR, 0},
		{cpu_regs::STATE_IMR, 0}, {cpu_regs::STATE_TCR, 0x10},
	});
	m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_B, 0x2345}, {cpu_regs::STATE_SP, 0x3000},
		{cpu_regs::STATE_BRC, f.count}, {cpu_regs::STATE_AR2, 0x5000}, {cpu_regs::STATE_AR3, 0x5100},
		{cpu_regs::STATE_AR4, 0x5300}, {cpu_regs::STATE_AR5, 0x5400},
	});
	for (unsigned i = 0; i <= f.count + 1; ++i) { data.write_word(0x5000 + i, f.value(i == f.exit)); data.write_word(0x5100 + i, 0xaaaa); data.write_word(0x5400 + i, f.value(i != f.exit)); }
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = m_pipeline_extensions_reads = m_fast_stack_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "block_exit_read", [this, f](offs_t address, u16 &value, u16) {
		if (machine().side_effects_disabled()) return;
		if (f.delayed && address >= 0x5400 && address <= 0x5408)
		{
			unsigned const i = m_fast_stack_reads++;
			expect(i < f.reads && address == 0x5400 + i && value == f.value(i != f.exit), "Each delayed exit consumes its opposite-condition input once"); return;
		}
		unsigned const i = m_pipeline_operand_reads++;
		expect(i < f.reads && address == 0x5000 + i && value == f.value(i == f.exit), "Block exit consumes each selected iteration once without speculative body reads");
		expect(m_cpu->state_int(tms320c54x_device::STATE_BRC) == f.count - i && bool(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x8000) == (i < f.count), "Block exit retains Decode-owned count and final-iteration BRAF");
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "block_exit_write", [this, f](offs_t address, u16 &value, u16) {
		if (machine().side_effects_disabled()) return;
		if (f.delayed && address >= 0x5300 && address <= 0x5308)
		{
			unsigned const i = m_pipeline_extensions_reads++;
			expect(i < f.reads && address == 0x5300 + i && value == f.value(f.reverse_delay ? i == f.exit : i != f.exit), "Retained delay words change the tested accumulator after the decision"); return;
		}
		if (address == 0x5200)
		{
			expect(f.taken && !m_pipeline_register_writes && value == 0xcccc && m_pipeline_operand_reads == f.reads && m_pipeline_store_writes == f.body_writes &&
				m_cpu->state_int(tms320c54x_device::STATE_BRC) == f.count - f.exit && bool(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x8000) == (f.exit < f.count), "Taken exit annuls its remaining body without automatically clearing BRAF or consuming BRC");
			++m_pipeline_register_writes; return;
		}
		unsigned const i = m_pipeline_store_writes++;
		expect(i < f.writes && address == 0x5100 + i && value == 0x1111 &&
			(i < f.body_writes || (m_pipeline_register_writes == 1 && !(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x8000))), "Only untaken iterations and one deliberate post-clear endpoint store may publish");
	});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62064;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_block_branch(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const interrupt = variant / 4096;
	bool const endpoint = (variant / 16) % 2, b = (variant / 32) % 2;
	u32 const base = (variant / 64) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, 0xf072); program.write_word(base + 0x601, endpoint ? 0x607 : 0x60d);
	program.write_word(base + 0x602, b ? 0x1192 : 0x1092);
	if (endpoint)
	{
		program.write_word(base + 0x603, 0x7693); program.write_word(base + 0x604, 0x1111);
		program.write_word(base + 0x605, b ? 0xf84d : 0xf845); program.write_word(base + 0x606, 0x607); program.write_word(base + 0x608, 0xf4e1);
	}
	else
	{
		program.write_word(base + 0x603, b ? 0xf84d : 0xf845); program.write_word(base + 0x604, 0x60a);
		program.write_word(base + 0x605, 0x7693); program.write_word(base + 0x606, 0x1111);
		program.write_word(base + 0x607, 0xf073); program.write_word(base + 0x608, 0x60c);
		program.write_word(base + 0x60a, 0x7693); program.write_word(base + 0x60b, 0x2222);
		program.write_word(base + 0x60c, b ? 0xf300 : 0xf000); program.write_word(base + 0x60d, 1);
	}
	// A later ordinary branch replaces the held tags before restoration,
	// so replay must recover the saved in-block tags rather than stale ones.
	unsigned const tail = endpoint ? 0x608 : 0x60e;
	program.write_word(base + tail, b ? 0xf84a : 0xf842); program.write_word(base + tail + 1, tail + 2); program.write_word(base + tail + 2, 0xf4e1);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_ST0, 0xa608}, {cpu_regs::STATE_ST1, interrupt == 1 ? 0x100 : 0x900},
	});
	m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, interrupt == 1 ? 4 : 0}, {cpu_regs::STATE_TCR, 0x10},
	});
	unsigned const vector = interrupt == 2 ? 4 : 0x48; program.write_word(base + vector, 0xf495); program.write_word(base + vector + 1, 0xf4eb);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1234); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x2345); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x5000); m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x5100); m_cpu->set_state_int(tms320c54x_device::STATE_BRC, block_branch_count(variant));
	for (unsigned i = 0; i <= block_branch_count(variant); ++i) { data.write_word(0x5000 + i, block_branch_source(variant, i)); data.write_word(0x5100 + i, 0xaaaa); }
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = m_fast_stack_reads = m_vector_acks = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "block_branch_read", [this, variant, interrupt](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == 0x2fff) { expect(interrupt && !m_fast_stack_reads && m_pipeline_register_writes == 1 && word >= 0x602 && word <= 0x610, "Block branch RETE consumes its retained foreground continuation once"); ++m_fast_stack_reads; return; } unsigned const i = m_pipeline_operand_reads++; expect(i <= block_branch_count(variant) && at == 0x5000 + i && word == block_branch_source(variant, i), "Block branches consume exactly count-plus-one ordered source words"); expect(m_cpu->state_int(tms320c54x_device::STATE_BRC) == block_branch_count(variant) - i && bool(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x8000) == (i < block_branch_count(variant)), "Branch redirects preserve Decode-owned BRC and final-iteration BRAF"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "block_branch_write", [this, variant, endpoint, interrupt](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == 0x2fff) { expect(interrupt && !m_pipeline_register_writes && word >= 0x602 && word <= 0x610, "Block branch interrupt stacks one unexecuted foreground continuation"); ++m_pipeline_register_writes; return; } unsigned const i = m_pipeline_store_writes++; bool const valid = i <= block_branch_count(variant) && at == 0x5100 + i && word == (!endpoint && !block_branch_source(variant, i) ? 0x2222 : 0x1111); if (!valid) osd_printf_info("block_branch_write: case=%u replay=%u i=%u address=%04x word=%04x cycle=%llu\n", variant, unsigned(m_pipeline_replay), i, unsigned(at), word, (unsigned long long)m_cpu->pipeline_cycles()); expect(valid, "Only the selected block arm publishes once per iteration"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254360;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_block_branch_edge(unsigned variant)
{
	start_block_branch(variant & 8 ? 64 : 0);
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove();
	u32 const base = variant & 8 ? 0x20000 : 0;
	unsigned const kind = variant % 8;
	m_cpu->space(AS_PROGRAM).write_word(base + 0x603, std::array<u16, 8>{0xf073, 0xf073, 0xf273, 0xf845, 0xf845, 0xf945, 0xf072, 0xf880}[kind]);
	m_cpu->space(AS_PROGRAM).write_word(base + 0x604, kind == 0 || kind == 3 ? 0x601 : kind == 1 || kind == 4 ? 0x700 : 0x60a);
	if (kind == 1 || kind == 4)
		m_cpu->space(AS_PROGRAM).write_word(base + 0x601, 0x604); // Branch extension at REA remains unvalidated.
	if (kind == 2)
	{
		// Internal BD is admitted, but its complete delay span must end
		// before REA; endpoint/control ownership is a separate contract.
		m_cpu->space(AS_PROGRAM).write_word(base + 0x601, 0x606);
		m_cpu->space(AS_PROGRAM).write_word(base + 0x604, 0x602);
	}
	m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "block_branch_edge_read", [this](offs_t at, u16 &, u16) { if (machine().side_effects_disabled()) return; expect(at == 0x5000 && !m_pipeline_operand_reads, "Rejected block control permits only an older legal source read"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "block_branch_edge_write", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) ++m_pipeline_store_writes; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254380;
	m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::delay_multiply_fixture tms320c54x_test_state::delay_square_case(unsigned variant)
{
	auto f = delay_accumulator_multiply_case(variant);
	f.square = true; f.destination = f.operation & 1; f.operation >>= 1;
	if (f.kind != 1 && f.kind != 2) f.a = f.b ^ 0x55;
	s64 const result = f.arithmetic();
	f.result = u64(f.flags & 2 ? std::clamp(result, s64(-2147483648LL), s64(2147483647LL)) : result) & 0xffffffffffULL;
	return f;
}

tms320c54x_test_state::block_xc_fixture tms320c54x_test_state::block_xc_case(unsigned v)
{
	return {v % 17, (v / 17) % 4, std::array<unsigned, 4>{0, 1, 2, 7}[(v / 68) % 4], (v / 1088) % 4, v / 4352, bool((v / 272) % 2), (v / 544) % 2 ? 0x20000U : 0};
}

void tms320c54x_test_state::start_block_xc(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant; auto const f = block_xc_case(variant);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	u16 const end = f.endpoint ? (f.layout == 3 ? 0x606 : 0x607) : 0x60b;
	program.write_word(f.base + 0x600, 0xf072); program.write_word(f.base + 0x601, end);
	program.write_word(f.base + 0x602, 0x1092 | (f.b() ? 0x100 : 0));
	u16 const predicate = f.predicate < 12 ? 0xfd42 + f.predicate % 6 + (f.b() ? 8 : 0) : f.predicate == 12 ? 0xfd20 : f.predicate == 13 ? 0xfd30 : f.predicate == 15 ? 0xfd0c : f.predicate == 16 ? 0xfd08 : 0xfd00;
	program.write_word(f.base + 0x605, predicate | (f.layout == 1 || f.layout == 2 ? 0x200 : 0));
	program.write_word(f.base + 0x606, f.layout == 2 ? 0x7693 : 0x8093 | (f.b() ? 0x100 : 0));
	program.write_word(f.base + 0x607, f.layout == 2 ? 0xfd43 : 0x8094 | (f.b() ? 0x100 : 0));
	if (f.layout == 2) program.write_word(f.base + 0x608, 0x8094 | (f.b() ? 0x100 : 0));
	program.write_word(f.base + end + 9, 0xf4e1);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_ST0, 0xa608 | (f.rotation ? 0x1800 : 0)}, {cpu_regs::STATE_ST1, 0x900},
		{cpu_regs::STATE_IMR, 0}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_TCR, 0x10},
		{cpu_regs::STATE_A, 0x123456789aULL}, {cpu_regs::STATE_B, 0xfedcba9876ULL}, {cpu_regs::STATE_BRC, f.count},
		{cpu_regs::STATE_AR2, 0x5000}, {cpu_regs::STATE_AR3, 0x5100}, {cpu_regs::STATE_AR4, 0x5200},
	});
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	for (unsigned i = 0; i <= f.count; ++i) data.write_word(0x5000 + i, f.word(i));
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "block_xc_read", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		unsigned const i = m_pipeline_operand_reads++;
		expect(i <= f.count && at == 0x5000 + i && word == f.word(i) && m_cpu->state_int(tms320c54x_device::STATE_BRC) == f.count - i && bool(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x8000) == (i < f.count), "Every XC iteration reads one source with its own counter and BRAF state");
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "block_xc_write", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at >= 0x5100 && at < 0x5200) { unsigned const i = f.selected(m_pipeline_store_writes); expect(i <= f.count && at == 0x5100 + m_pipeline_store_writes++ && word == (f.layout == 2 ? 0xfd43 : f.word(i)), "Only selected first slots write in source order, including a captured opcode-shaped literal"); return; }
		unsigned const i = f.layout == 1 ? f.selected(m_pipeline_register_writes) : f.layout >= 2 && f.endpoint ? f.count : m_pipeline_register_writes;
		expect(i <= f.count && at == 0x5200 + m_pipeline_register_writes++ && word == f.word(i), "Second slots preserve XC1 fallthrough, XC2 selection and post-loop continuation");
	});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 255680;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_block_xc_edge(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	u32 const base = (variant / 3) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 12; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, 0xf072); program.write_word(base + 0x601, 0x602 + (variant % 3 == 2)); program.write_word(base + 0x602, variant % 3 ? 0xff00 : 0xfd00);
	program.write_word(base + 0x603, 0x8c93); program.write_word(base + 0x604, 0x8d94);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_ST0, 0xa608}, {cpu_regs::STATE_ST1, 0x900}, {cpu_regs::STATE_IMR, 0},
		{cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_BRC, 3}, {cpu_regs::STATE_AR3, 0x5100},
		{cpu_regs::STATE_AR4, 0x5200},
	});
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "block_xc_edge_no_read", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Cross-end XC exclusion has no data reads"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "block_xc_edge_no_write", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Cross-end XC exclusion has no data writes"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 255700;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_move_repeat_owner(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	u32 const base = variant & 2 ? 0x20000 : 0;
	u16 const value = variant & 4 ? 0x5aa5 : 0x1400, count = variant & 8 ? 3 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, 0xf495);
	unsigned cursor = 0x600;
	auto move = [&]() { program.write_word(base + cursor++, 0x710f); program.write_word(base + cursor++, 0x12); };
	auto repeat = [&]() { program.write_word(base + cursor++, variant & 16 ? 0xf070 : 0xec00 | count); if (variant & 16) program.write_word(base + cursor++, count); };
	if (!(variant & 1)) move();
	repeat();
	if (variant & 1) move(); else program.write_word(base + cursor++, 0xf495);
	program.write_word(base + cursor, 0xf4e1);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ST0, 0},
		{cpu_regs::STATE_ST1, 0x800}, {cpu_regs::STATE_IMR, 0}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_TCR, 0x10},
		{cpu_regs::STATE_TRN, value}, {cpu_regs::STATE_AR2, 0x3333},
	});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 299000;
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_block_back(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant; auto const f = block_back_case(variant);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, 0xf072); program.write_word(f.base + 0x601, f.end);
	program.write_word(f.base + 0x602, f.b ? 0xf120 : 0xf020); program.write_word(f.base + 0x603, f.inner - 1);
	program.write_word(f.base + 0x604, 0xe589); program.write_word(f.base + 0x605, f.b ? 0xf300 : 0xf000); program.write_word(f.base + 0x606, 0xffff);
	program.write_word(f.base + 0x607, (f.b ? 0xf84a : 0xf842) | (f.kind ? 1 : 0)); program.write_word(f.base + 0x608, f.kind ? 0x60c : 0x604);
	if (f.kind) { program.write_word(f.base + 0x609, 0xf073); program.write_word(f.base + 0x60a, 0x604); }
	program.write_word(f.base + f.end - 1, 0x6d94); program.write_word(f.base + f.end + 1, 0xf4e1);
	unsigned const vector = f.interrupt == 2 ? 4 : 0x48; program.write_word(f.base | vector, 0xf495); program.write_word((f.base | vector) + 1, 0xf4eb);
	for (unsigned i = 0; i < 128; ++i) { data.write_word(0x5000 + i, 0x9000 + i); data.write_word(0x6000 + i, 0); }
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, f.interrupt == 1 ? 4 : 0},
		{cpu_regs::STATE_ST0, 0xa002}, {cpu_regs::STATE_ST1, f.st1}, {cpu_regs::STATE_BRC, f.outer},
		{cpu_regs::STATE_A, 0x123456789aULL}, {cpu_regs::STATE_B, 0xfeabcd9876ULL}, {cpu_regs::STATE_T, 0x5555},
		{cpu_regs::STATE_SP, 0x3000}, {cpu_regs::STATE_AR2, 0x5000}, {cpu_regs::STATE_AR3, 0x6000},
		{cpu_regs::STATE_AR4, 0x7000},
	});
	m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = m_fast_stack_reads = m_vector_acks = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "block_back_read", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == 0x2fff) { expect(f.interrupt && !m_fast_stack_reads && m_pipeline_register_writes == 1 && word >= 0x602 && word <= f.end + 1, "Backward block IRQ restores one foreground continuation"); ++m_fast_stack_reads; return; } unsigned const i = m_pipeline_operand_reads; expect(i < (f.outer + 1) * f.inner && at == 0x5000 + i && word == 0x9000 + i, "Backward inner loop reads every source once without consuming BRC"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "block_back_write", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == 0x2fff) { expect(f.interrupt && !m_pipeline_register_writes && word >= 0x602 && word <= f.end + 1, "Backward block IRQ stacks one retained continuation"); ++m_pipeline_register_writes; return; } unsigned const i = m_pipeline_store_writes; expect(i < (f.outer + 1) * f.inner && at == 0x6000 + i && word == 0x9000 + i, "Backward inner loop writes only its ordered selected copies"); ++m_pipeline_store_writes; if (!i && f.interrupt) m_cpu->set_input_line(f.interrupt == 2 ? INPUT_LINE_NMI : 2, ASSERT_LINE); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 255240;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::block_page_fixture tms320c54x_test_state::block_page_case(unsigned variant)
{
	bool const delayed = variant >= 192; variant %= 192;
	unsigned const page = (variant / 8) % 3;
	return {std::array<unsigned, 4>{0, 1, 2, 7}[variant % 4],
		std::array<unsigned, 4>{6, 10, 16, 24}[variant / 48],
		std::array<u32, 3>{0, 0x20000, 0x7f0000}[page],
		std::array<u32, 3>{0x20000, 0x7f0000, 0}[page],
		bool((variant / 4) % 2), bool((variant / 24) % 2), delayed};
}

void tms320c54x_test_state::start_block_page(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = block_page_case(variant); auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) { program.write_word(f.caller + 0x600 + i, 0xf495); program.write_word(f.callee + 0x600 + i, 0xf495); }
	program.write_word(f.caller + 0x600, 0xf072); program.write_word(f.caller + 0x601, 0x609);
	program.write_word(f.caller + 0x602, 0x6d94);
	program.write_word(f.caller + 0x603, (f.delayed ? 0xfb80 : 0xf980) | (f.callee >> 16)); program.write_word(f.caller + 0x604, f.hazard ? 0x608 : 0x610);
	if (f.delayed) program.write_word(f.caller + 0x605, 0x6d97);
	program.write_word(f.caller + 0x60a, 0xf4e1);
	program.write_word(f.callee + 0x602, 0x6d95);
	program.write_word(f.callee + 0x609, 0x6d96); program.write_word(f.callee + 0x60a, 0xf4e4);
	program.write_word(f.callee + 0x610, 0x6d96); program.write_word(f.callee + 0x611, 0xf4e4);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_TCR, 0x10},
		{cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0}, {cpu_regs::STATE_ST0, 0xa608},
		{cpu_regs::STATE_ST1, 0x900}, {cpu_regs::STATE_BRC, f.count}, {cpu_regs::STATE_SP, 0x3000},
		{cpu_regs::STATE_AR4, 0x1400}, {cpu_regs::STATE_AR5, 0x1500}, {cpu_regs::STATE_AR6, 0x1600},
		{cpu_regs::STATE_AR7, 0x1700},
	});
	m_pipeline_store_writes = m_pipeline_operand_reads = m_idle_fixture_tick = 0; m_pipeline_replay = false;
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "block_page_frame_writes",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			unsigned const part = m_pipeline_store_writes++ % 2;
			expect(at == 0x2fff - part && word == (part ? f.caller >> 16 : f.delayed ? 0x607 : 0x605), "Cross-page FCALL publishes one ordered PC/page frame"); });
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "block_page_frame_reads",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			unsigned const part = m_pipeline_operand_reads++ % 2;
			expect(at == 0x2ffe + part && word == (part ? f.delayed ? 0x607 : 0x605 : f.caller >> 16), "Cross-page FRET consumes the matching page/PC frame"); });
	m_cpu->set_state_int(STATE_GENPC, f.caller + 0x600); m_cpu->set_pipeline_frontend(f.staged);
	m_phase = 294000; m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::block_far_call_fixture tms320c54x_test_state::block_far_call_case(unsigned v, bool delayed)
{
	block_far_call_fixture f{};
	f.delayed = delayed; f.delay_form = delayed ? v / 768 : 0; v %= 768;
	f.count = std::array<unsigned, 4>{0, 1, 2, 7}[v % 4]; f.layout = (v / 4) % 2; f.caller = (v / 8) % 2 ? 0x20000 : 0;
	f.callee = (((v / 16) % 2 ? f.caller ^ 0x20000 : f.caller) | 0x700); f.nested = (f.callee ^ 0x20000) & 0x7f0000; f.nested |= 0x800;
	f.depth = 1 + (v / 32) % 2; f.interrupt = (v / 64) % 3; f.checkpoint = std::array<unsigned, 4>{8, 10, 15, 23}[v / 192];
	f.call = f.layout ? 0x606 : 0x603; f.st1 = f.interrupt ? 0x100 : 0x900;
	return f;
}

void tms320c54x_test_state::start_block_far_call(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = block_far_call_case(variant, profile_is("rptb-fcalld"));
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(f.caller + 0x600 + i, 0xf495);
	program.write_word(f.caller + 0x600, 0xf072); program.write_word(f.caller + 0x601, 0x609); program.write_word(f.caller + 0x602, 0x6d94);
	program.write_word(f.caller + f.call, (f.delayed ? 0xfb80 : 0xf980) | (f.callee >> 16)); program.write_word(f.caller + f.call + 1, 0x700);
	program.write_word(f.caller + f.call + 2, !f.delayed || !f.delay_form ? 0x6d95 : f.delay_form == 1 ? 0x770e : 0xe801);
	if (f.delayed) program.write_word(f.caller + f.call + 3, !f.delay_form ? 0x6d97 : f.delay_form == 1 ? 0x1234 : 0xf495);
	program.write_word(f.caller + 0x60a, 0xf4e1);
	program.write_word(f.callee, 0xe589);
	program.write_word(f.callee + 1, f.depth == 2 ? 0xf980 | (f.nested >> 16) : 0xf4e4);
	program.write_word(f.callee + 2, 0x800); program.write_word(f.callee + 3, 0xf4e4);
	program.write_word(f.nested, 0x6d96); program.write_word(f.nested + 1, 0xf4e4);
	unsigned const vector = f.interrupt == 2 ? 4 : 0x48;
	for (u32 base : {0U, 0x20000U}) { program.write_word(base | vector, 0xf495); program.write_word((base | vector) + 1, 0xf4eb); }
	for (unsigned i = 0; i < 16; ++i) { data.write_word(0x5000 + i, 0x9000 + i); data.write_word(0x6000 + i, 0); }
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, f.interrupt == 1 ? 4 : 0},
		{cpu_regs::STATE_ST0, 0xa608}, {cpu_regs::STATE_ST1, f.st1}, {cpu_regs::STATE_BRC, f.count},
		{cpu_regs::STATE_A, 0x123456789aULL}, {cpu_regs::STATE_B, 0xfeabcd9876ULL}, {cpu_regs::STATE_T, 0x5555},
		{cpu_regs::STATE_SP, 0x3000},
	});
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, i == 2 ? 0x5000 : i == 3 ? 0x6000 : i == 4 ? 0x7000 : i == 5 ? 0x7100 : 0x7200 + i);
	m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = m_fast_stack_reads = m_vector_acks = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "block_far_call_read", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at >= 0x2ff0 && at < 0x3000)
		{
			if (at == 0x2ffe)
			{
				auto const disable = machine().disable_side_effects();
				expect(word == f.caller >> 16 && m_cpu->space(AS_DATA).read_word(0x2fff) == f.call + (f.delayed ? 4 : 2), "Outer FRET consumes the paired caller page and actual post-delay continuation");
			}
			++m_fast_stack_reads; expect(m_fast_stack_reads <= m_pipeline_register_writes, "Block callee returns consume only published stack words"); return;
		}
		unsigned const i = m_pipeline_operand_reads; expect(i <= f.count && at == 0x5000 + i && word == 0x9000 + i, "Block callee reads one ordered source per caller iteration"); ++m_pipeline_operand_reads;
		if (!i && f.interrupt) m_cpu->set_input_line(f.interrupt == 2 ? INPUT_LINE_NMI : 2, ASSERT_LINE);
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "block_far_call_write", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at >= 0x2ff0 && at < 0x3000)
		{
			// An IRQ after FRET can reuse 2fff. Check the two-word call
			// as a page/PC pair, not every write to that stack address.
			if (at == 0x2ffe)
			{
				auto const disable = machine().disable_side_effects();
				expect(word == f.caller >> 16 && m_cpu->space(AS_DATA).read_word(0x2fff) == f.call + (f.delayed ? 4 : 2), "Outer FCALL publishes the paired caller page and actual post-delay continuation");
			}
			++m_pipeline_register_writes; return;
		}
		if (f.delayed && f.delay_form == 1 && at == 0x0e) { expect(word == 0x1234 && m_pipeline_extensions_reads <= f.count, "Two-word delay owns one T publication per block iteration"); ++m_pipeline_extensions_reads; return; }
		unsigned const i = m_pipeline_store_writes; expect(i <= f.count && at == 0x6000 + i && word == 0x9000 + i, "Block callee writes each copied word once"); ++m_pipeline_store_writes;
	});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 255200;
	m_cpu->set_state_int(STATE_GENPC, f.caller + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}


tms320c54x_test_state::block_near_call_fixture tms320c54x_test_state::block_near_call_case(unsigned v)
{
	block_near_call_fixture f{};
	f.count = std::array<unsigned, 4>{0, 1, 5, 15}[v % 4]; v /= 4;
	f.layout = v % 3; v /= 3; f.base = std::array<u32, 3>{0, 0x20000, 0x7f0000}[v % 3]; v /= 3;
	f.depth = 1 + v % 2; v /= 2; f.interrupt = v % 3; v /= 3;
	f.delayed_return = v % 2; v /= 2; f.delayed = v % 2; v /= 2;
	f.checkpoint = std::array<unsigned, 4>{5, 8, 13, 25}[v];
	f.call = f.layout == 0 ? 0x603 : f.layout == 1 ? (f.delayed ? 0x606 : 0x608) : 0x602;
	f.end = f.layout == 2 ? f.call + (f.delayed ? 3 : 1) : 0x609;
	f.st1 = f.interrupt ? 0x100 : 0x900;
	return f;
}

void tms320c54x_test_state::start_block_near_call(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	m_observed_instructions.clear();
	auto const f = block_near_call_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) { program.write_word(f.base + 0x600 + i, 0xf495); program.write_word(f.base + 0x700 + i, 0xf495); program.write_word(f.base + 0x800 + i, 0xf495); }
	program.write_word(f.base + 0x600, 0xf072); program.write_word(f.base + 0x601, f.end);
	if (f.layout != 2) program.write_word(f.base + 0x602, 0x6d94);
	program.write_word(f.base + f.call, f.delayed ? 0xf274 : 0xf074); program.write_word(f.base + f.call + 1, 0x700);
	if (f.delayed || f.layout == 0) program.write_word(f.base + f.call + 2, 0x6d95);
	// A pending IRQ can legitimately wake an adjacent IDLE (SPRU131G
	// 6.11.1). Check fallthrough once, then drain before parking.
	program.write_word(f.base + f.end + 1, 0x6d91); program.write_word(f.base + f.end + 10, 0xf4e1);
	program.write_word(f.base + 0x700, 0xe589);
	if (f.depth == 2) { program.write_word(f.base + 0x701, 0xf074); program.write_word(f.base + 0x702, 0x800); }
	unsigned const ret = f.depth == 2 ? 0x703 : 0x701;
	program.write_word(f.base + ret, f.delayed_return ? 0xfe00 : 0xfc00);
	if (f.delayed_return) program.write_word(f.base + ret + 1, 0x6d97);
	program.write_word(f.base + 0x800, 0x6d96); program.write_word(f.base + 0x801, f.delayed_return ? 0xfe00 : 0xfc00);
	if (f.delayed_return) program.write_word(f.base + 0x802, 0x6d97);
	unsigned const vector = f.interrupt == 2 ? 4 : 0x48;
	program.write_word(f.base | vector, 0xf495); program.write_word((f.base | vector) + 1, 0xf4eb);
	for (unsigned i = 0; i < 16; ++i) { data.write_word(0x5000 + i, 0x9000 + i); data.write_word(0x6000 + i, 0); }
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, f.interrupt == 1 ? 4 : 0},
		{cpu_regs::STATE_ST0, 0xa608}, {cpu_regs::STATE_ST1, f.st1}, {cpu_regs::STATE_BRC, f.count},
		{cpu_regs::STATE_A, 0x123456789aULL}, {cpu_regs::STATE_B, 0xfeabcd9876ULL}, {cpu_regs::STATE_T, 0x5555},
		{cpu_regs::STATE_SP, 0x3000},
	});
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, i == 2 ? 0x5000 : i == 3 ? 0x6000 : 0x6c00 + 0x100 * i);
	m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = m_fast_stack_reads = m_vector_acks = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "block_near_call_read", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at >= 0x2ff0 && at < 0x3000) { if (!f.interrupt && at == 0x2fff) expect(word == f.call + (f.delayed ? 4 : 2), "Near RET consumes the ordinary post-call continuation"); ++m_fast_stack_reads; expect(m_fast_stack_reads <= m_pipeline_register_writes, "Near RET consumes only committed frames"); return; }
		unsigned const i = m_pipeline_operand_reads++;
		if (i > f.count || at != 0x5000 + i || word != 0x9000 + i)
		{
			osd_printf_info("block_near_call_read: case=%u index=%u address=%04x value=%04x pc=%llx clock=%llu sp=%llx brc=%llx\n", m_pipeline_peripheral_case, i, unsigned(at), word, (unsigned long long)m_cpu->state_int(STATE_GENPC), (unsigned long long)m_cpu->pipeline_cycles(), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_SP), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_BRC));
			for (auto const &entry : m_observed_instructions) osd_printf_info("block_near_call_retired: pc=%06x word=%04x\n", unsigned(entry.first), entry.second);
		}
		expect(i <= f.count && at == 0x5000 + i && word == 0x9000 + i, "Block near callee reads one ordered word per iteration");
		if (!i && f.interrupt) m_cpu->set_input_line(f.interrupt == 2 ? INPUT_LINE_NMI : 2, ASSERT_LINE);
	});
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "block_near_call_write", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at >= 0x2ff0 && at < 0x3000) { if (!f.interrupt && at == 0x2fff) expect(word == f.call + (f.delayed ? 4 : 2), "Near CALL pushes the ordinary post-call continuation"); ++m_pipeline_register_writes; return; }
		unsigned const i = m_pipeline_store_writes++; expect(i <= f.count && at == 0x6000 + i && word == 0x9000 + i, "Block near callee writes one ordered word per iteration");
	});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62150;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}


tms320c54x_test_state::block_single_fixture tms320c54x_test_state::block_single_case(unsigned v) const
{
	block_single_fixture f{};
	f.memory = profile_is("rptb-memory");
	if (f.memory) { f.count_form = v % 5; v /= 5; }
	f.outer = std::array<unsigned, 4>{0, 1, 2, 7}[v % 4]; f.inner = std::array<unsigned, 4>{0, 1, 2, 7}[(v / 4) % 4];
	f.kind = (v / 16) % 3; f.tail = (v / 48) % 2; f.base = (v / 96) % 2 ? 0x20000 : 0;
	f.checkpoint = std::array<unsigned, 4>{7, 10, 13, 17}[v / 192]; f.total = (f.outer + 1) * (f.inner + 1);
	f.end = 0x604 + unsigned(f.kind == 1) + (f.tail ? 0 : 3);
	if (f.memory && f.count_form == 3) f.total += (f.outer + 1) / 2;
	if (f.memory && f.count_form == 4) { ++f.end; ++f.body; }
	return f;
}

void tms320c54x_test_state::start_block_single(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant; auto const f = block_single_case(variant);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, 0xf072); program.write_word(f.base + 0x601, f.end);
	program.write_word(f.base + 0x602, 0x6d94); program.write_word(f.base + 0x603, 0xec00 | f.inner);
	program.write_word(f.base + 0x604, std::array<u16, 3>{0xe589, 0x7693, 0xf495}[f.kind]);
	if (f.kind == 1) program.write_word(f.base + 0x605, 0x1234);
	if (!f.tail) program.write_word(f.base + 0x605 + unsigned(f.kind == 1), 0x6d95);
	if (f.memory)
	{
		program.write_word(f.base + 0x603, std::array<u16, 5>{0x4760, 0x4760, 0x4780, 0x4790, 0x47f8}[f.count_form]);
		if (f.count_form == 4) program.write_word(f.base + 0x604, 0x1060);
		program.write_word(f.base + f.body, std::array<u16, 3>{0xe589, 0x7693, 0xf495}[f.kind]);
		if (f.kind == 1) program.write_word(f.base + f.body + 1, 0x1234);
		if (!f.tail) program.write_word(f.base + f.body + 1 + unsigned(f.kind == 1), 0x6d95);
		for (unsigned i = 0; i <= f.outer; ++i) data.write_word(0x1060 + i, f.inner + (f.count_form == 3 ? i & 1 : 0));
	}
	program.write_word(f.base + f.end + 1, 0x770e); program.write_word(f.base + f.end + 2, 0x1a2b); program.write_word(f.base + f.end + 3, 0xf4e1);
	for (unsigned i = 0; i < 128; ++i) { data.write_word(0x5000 + i, 0x9000 + i); data.write_word(0x6000 + i, 0); }
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0}, {cpu_regs::STATE_ST0, 0x8820},
		{cpu_regs::STATE_ST1, 0x800}, {cpu_regs::STATE_BRC, f.outer}, {cpu_regs::STATE_RC, 0},
		{cpu_regs::STATE_A, 0x123456789aULL}, {cpu_regs::STATE_B, 0xfeabcd9876ULL}, {cpu_regs::STATE_T, 0x5555},
		{cpu_regs::STATE_SP, 0x1000},
	});
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, i == 2 ? 0x5000 : i == 3 ? 0x6000 : i == 4 ? 0x7000 : i == 5 ? 0x7100 : 0x7200 + i);
	if (f.memory)
	{
		m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0x1060);
		if (f.count_form == 1) m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x4800);
	}
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0x5000, 0x50ff, "block_single_read", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; unsigned const i = m_pipeline_operand_reads; expect(f.kind == 0 && i < f.total && at == 0x5000 + i && word == 0x9000 + i, "Inner MVDD reads each outer/inner source once in order"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0x6000, 0x60ff, "block_single_write", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; unsigned const i = m_pipeline_store_writes; expect(f.kind < 2 && i < f.total && at == 0x6000 + i && word == (f.kind == 1 ? 0x1234 : 0x9000 + i), "Inner repeat writes each destination once without consuming outer iterations"); ++m_pipeline_store_writes; });
	m_pipeline_extensions_reads = 0;
	if (f.memory) m_pipeline_fetch_tap = data.install_read_tap(0x1060, 0x1067, "block_memory_count", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		unsigned const i = m_pipeline_extensions_reads++;
		expect(i <= f.outer && at == 0x1060 + (f.count_form == 3 ? i : 0) && word == f.inner + (f.count_form == 3 ? i & 1 : 0), "Each outer iteration reads its original memory count exactly once");
		if (f.count_form == 3) m_cpu->space(AS_DATA).write_word(at, 0xffff);
	});
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 255160;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}


tms320c54x_test_state::branch_repeat_fixture tms320c54x_test_state::branch_repeat_case(unsigned v) const
{
	branch_repeat_fixture f{};
	f.conditional = profile_is("bcd-rpt");
	if (f.conditional) { f.condition = v % 12; v /= 12; }
	f.staged = v % 2; v /= 2; f.kind = v % 3; v /= 3;
	f.count = std::array<unsigned,5>{0,1,8,39,255}[v % 5]; v /= 5;
	f.following = std::array<unsigned,3>{0,2,7}[v % 3]; v /= 3;
	f.target = v % 2 ? 0x500 : 0x700; v /= 2; unsigned const pattern = v % 4; v /= 4;
	f.seed = std::array<u16,4>{0,0xffff,0x8001,0x0fff}[pattern]; f.status = 0x800 | (pattern << 8);
	f.predicate_pattern = pattern;
	f.base = v % 2 ? 0x20000 : 0; v /= 2;
	f.checkpoint = std::array<unsigned,4>{3,8,9+f.count/2,20+f.count}[v % 4];
	f.limit = 4*(f.count+f.following+2)+100; return f;
}

void tms320c54x_test_state::start_branch_repeat(unsigned variant)
{
	bool const returning = profile_is("retd-rpt");
	bool const calling = profile_is("calld-rpt");
	m_cpu->suspend(SUSPEND_REASON_DISABLE,true); m_pipeline_peripheral_case = variant;
	auto const f = branch_repeat_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove();
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base+0x600+i,0xf495);
	program.write_word(f.base+0x600,0xf273); program.write_word(f.base+0x601,f.target);
	if (calling) program.write_word(f.base+0x600,0xf274);
	if (f.conditional) program.write_word(f.base+0x600,0xfa40 | (f.condition < 6 ? f.condition + 2 : f.condition + 4));
	program.write_word(f.base+0x602,0xec00|f.count); program.write_word(f.base+0x603,f.body()); program.write_word(f.base+0x604,0xf4e1);
	if (returning)
	{
		program.write_word(f.base+0x600,0xfe00); program.write_word(f.base+0x601,0xec00|f.count);
		program.write_word(f.base+0x602,f.body()); program.write_word(f.base+0x603,0xf495);
		data.write_word(0x2fff,f.target);
	}
	program.write_word(f.base+f.target,0xec00|f.following); program.write_word(f.base+f.target+1,0x8193); program.write_word(f.base+f.target+2,0xf4e1);
	if (calling) program.write_word(f.base+f.target+2,0xfc00);
	if (f.conditional) { program.write_word(f.base+0x604,0xec00|f.following); program.write_word(f.base+0x605,0x8193); program.write_word(f.base+0x606,0xf4e1); }
	for (unsigned i = 0; i < 256; ++i) { data.write_word(0x1800+i,u16(f.seed+i*257)); data.write_word(0x1c00+i,0xdead); }
	for (unsigned i = 0; i < 32; ++i) data.write_word(0x2000+i,0xdead);
	for (u16 address : {0x17ff,0x1900,0x1bff,0x1d00,0x1fff,0x2020}) data.write_word(address,0x2468);
	m_cpu->set_pipeline_frontend(false);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE,0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL,0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST,0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0,0xa608); m_cpu->set_state_int(tms320c54x_device::STATE_ST1,f.status);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR,0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR,0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR,0);
	m_cpu->set_state_int(tms320c54x_device::STATE_A,f.a()); m_cpu->set_state_int(tms320c54x_device::STATE_B,f.b());
	m_cpu->set_state_int(tms320c54x_device::STATE_SP,0x3000); m_cpu->set_state_int(tms320c54x_device::STATE_T,0x5aa5);
	if (returning) m_cpu->set_state_int(tms320c54x_device::STATE_SP,0x2fff);
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC,13); m_cpu->set_state_int(tms320c54x_device::STATE_RSA,0x5555); m_cpu->set_state_int(tms320c54x_device::STATE_REA,0x6666); m_cpu->set_state_int(tms320c54x_device::STATE_RC,0);
	for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0+ar,ar == 3 ? 0x2000 : ar == 4 ? 0x1800 : ar == 5 ? 0x1c00 : 0x4000+ar);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_rptbd_counts.reads = 0;
	m_pipeline_register_writes = 0;
	m_observed_instructions.clear(); m_saved_observed_instructions.clear(); m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_operand_tap = data.install_read_tap(0,0xffff,"branch_repeat_read",[this,f,returning,calling](offs_t at,u16 &word,u16) {
		if (machine().side_effects_disabled()) return;
		if ((returning || calling) && at == 0x2fff)
		{
			expect(!m_rptbd_counts.reads++ && word == (calling ? 0x604 : f.target), "Return samples its owned original continuation once");
			return;
		}
		unsigned const i = m_pipeline_operand_reads++;
		expect(f.kind == 1 && i <= f.count && at == 0x1800+i && word == u16(f.seed+i*257),"Delay-owned MVDD reads exactly its ordered original source words");
	});
	m_pipeline_write_tap = data.install_write_tap(0,0xffff,"branch_repeat_write",[this,f,calling](offs_t at,u16 &word,u16) {
		if (machine().side_effects_disabled()) return;
		if (calling && at == 0x2fff) { expect(!m_pipeline_register_writes++ && word == 0x604 && !m_pipeline_store_writes && !m_pipeline_extensions_reads, "CALLD publishes one continuation before delay-body and target stores"); return; }
		if (at >= 0x2000 && at < 0x2020)
		{
			unsigned const i = m_pipeline_extensions_reads++;
			expect(i <= f.following && at == 0x2000+i && word == u16(f.b(true)) && m_pipeline_store_writes == (f.kind == 2 ? 0 : f.count+1),"Target repeat starts only after the entire delay-owned repeat has completed");
		}
		else
		{
			unsigned const i = m_pipeline_store_writes++;
			expect(f.kind != 2 && i <= f.count && !m_pipeline_extensions_reads && at == (f.kind ? 0x1c00 : 0x1800)+i && word == (f.kind ? u16(f.seed+i*257) : u16(f.a(true))),"Repeated delay body writes before any target-side publication");
		}
	});
	if (f.conditional && f.staged) m_pipeline_fetch_tap = program.install_read_tap(f.base+0x603,f.base+0x603,"bcd_repeat_body",[this,f](offs_t at,u16 &word,u16) { if (machine().side_effects_disabled()) return; expect(word == f.body(), "BCD repeat captures its original delay opcode once"); auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(at,0xe8ff); });
	m_phase = 600040; m_cpu->set_state_int(STATE_GENPC,f.base+0x600); m_cpu->set_pipeline_frontend(f.staged); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock()));
}

void tms320c54x_test_state::start_block_delay_repeat(unsigned variant)
{
	start_branch_repeat(variant);
	auto const f = branch_repeat_case(variant);
	auto &program = m_cpu->space(AS_PROGRAM);
	unsigned const end = f.target == 0x500 ? 0x604 : 0x605;
	write_words(program, {
		{f.base + 0x600, 0xf272}, {f.base + 0x601, end}, {f.base + 0x604, 0xf495}, {f.base + end, 0x8193},
		{f.base + end + 1, 0xf4e1},
	});
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC, f.following);
	m_phase = 600060;
}

void tms320c54x_test_state::start_branch_repeat_edge(unsigned variant)
{
	u32 const base = (variant/8)%2 ? 0x20000 : 0; unsigned const kind = variant%8;
	start_branch_repeat(1+(base ? 720 : 0)); m_cpu->suspend(SUSPEND_REASON_DISABLE,true); m_pipeline_peripheral_case = variant;
	auto &program = m_cpu->space(AS_PROGRAM);
	if (kind == 0) { program.write_word(base+0x602,0xf495); program.write_word(base+0x603,0xec01); }
	if (kind == 1) program.write_word(base+0x600,0x6e87);
	if (kind == 2) { program.write_word(base+0x600,0xf274); program.write_word(base+0x602,0xf495); program.write_word(base+0x603,0xec01); }
	if (kind == 3) { program.write_word(base+0x600,0xfe00); program.write_word(base+0x601,0xf495); program.write_word(base+0x602,0xec01); }
	if (kind == 4) program.write_word(base+0x602,0xf070);
	if (kind == 5) program.write_word(base+0x602,0x4780);
	if (kind >= 6) { program.write_word(base+0x602,0xec01); program.write_word(base+0x603,kind == 6 ? 0x76f8 : 0x71f8); }
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove();
	if (kind == 3) m_cpu->space(AS_DATA).write_word(0x3000,0x700);
	// RETD reads its return word before releasing delay Decode; CALLD
	// already reserved SP in Decode. Neither belongs to the rejected RPT.
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0,0xffff,"branch_repeat_edge_reads",[this,kind](offs_t at,u16 &word,u16) { if (!machine().side_effects_disabled()) { expect(kind == 3 && at == 0x3000 && word == 0x700 && !m_pipeline_operand_reads,"Only RETD's preceding return-word Read may occur before rejection"); ++m_pipeline_operand_reads; } });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0,0xffff,"branch_repeat_edge_writes",[this](offs_t,u16 &,u16) { if (!machine().side_effects_disabled()) ++m_pipeline_store_writes; });
	m_phase = 600050; m_cpu->set_state_int(STATE_GENPC,base+0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
}

void tms320c54x_test_state::start_block_delay_repeat_edge(unsigned ordinal)
{
	unsigned const kind = std::array<unsigned, 5>{0, 4, 5, 6, 7}[ordinal % 5];
	start_branch_repeat_edge(kind + (ordinal / 5) * 8);
	u32 const base = (ordinal / 5) % 2 ? 0x20000 : 0;
	m_cpu->space(AS_PROGRAM).write_word(base + 0x600, 0xf272);
	m_cpu->space(AS_PROGRAM).write_word(base + 0x601, 0x606);
}

void tms320c54x_test_state::start_conditional_delay_repeat_edge(unsigned ordinal)
{
	unsigned const kind = std::array<unsigned,5>{0,4,5,6,7}[ordinal % 5];
	start_branch_repeat_edge(kind + (ordinal / 5) * 8);
	u32 const base = (ordinal / 5) % 2 ? 0x20000 : 0;
	m_cpu->space(AS_PROGRAM).write_word(base + 0x600,0xfa47);
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

void tms320c54x_test_state::start_delayed_far_call(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const layout = variant % 3, caller_page = std::array<unsigned, 3>{0, 2, 0x7f}[(variant / 3) % 3], target_page = std::array<unsigned, 3>{0, 2, 0x7f}[(variant / 9) % 3];
	u32 const caller = caller_page << 16, target = target_page << 16;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 64; ++i) { program.write_word(caller + 0x600 + i, 0xf495); program.write_word(target + 0x700 + i, 0xf495); program.write_word(caller + i, 0xf495); program.write_word(target + i, 0xf495); }
	program.write_word(caller + 0x600, 0xfb80 | target_page); program.write_word(caller + 0x601, 0x700);
	program.write_word(caller + 0x602, layout == 0 ? 0x481e : layout == 1 ? 0x770e : 0xf495);
	program.write_word(caller + 0x603, layout == 0 ? 0x891a : layout == 1 ? 0x1234 : 0x4816);
	program.write_word(caller + 0x604, 0xe955); program.write_word(target + 0x700, 0xe933); program.write_word(target + 0x703, 0xf4e4);
	for (u32 page : {caller, target}) { program.write_word(page + 4, 0xf4eb); program.write_word(page + 0x40, 0xf4eb); }
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_ST0, 0xa608}, {cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 1},
		{cpu_regs::STATE_SP, 0x1000}, {cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0x1234},
		{cpu_regs::STATE_T, 0xbeef}, {cpu_regs::STATE_BRC, 0x9876}, {cpu_regs::STATE_AR6, 0x4567},
	});
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_pipeline_store_writes = m_fast_stack_reads = m_pipeline_extensions_reads = m_pipeline_operand_reads = m_pipeline_drained_fetches = m_vector_acks = m_pipeline_register_writes = 0;
	m_pipeline_write_tap = data.install_write_tap(0xffd, 0xfff, "delayed_far_call_stack",
		[this, caller_page](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (m_pipeline_store_writes < 2) expect(at == 0xfff - m_pipeline_store_writes && word == (m_pipeline_store_writes ? caller_page : 0x604), "FCALLD stacks the caller page and continuation after both delay words"); else expect(m_pipeline_store_writes == 2 && at == 0xffd, "Pending interrupt owns a separate stack word"); ++m_pipeline_store_writes; });
	m_pipeline_operand_tap = data.install_read_tap(0xffd, 0xfff, "delayed_far_call_return",
		[this, caller_page, variant](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; bool const interrupt = (variant / 27) % 3; if (interrupt && !m_fast_stack_reads) expect(at == 0xffd, "RETE removes only the interrupt frame"); else { unsigned const part = m_fast_stack_reads - unsigned(interrupt); expect(part < 2 && at == 0xffe + part && word == (part ? 0x604 : caller_page), "FRET restores the retained caller page and post-delay PC"); } ++m_fast_stack_reads; });
	m_pipeline_fetch_tap = program.install_read_tap(caller + 0x601, caller + 0x604, "delayed_far_call_words",
		[this, caller, layout](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == caller + 0x601) { expect(!m_pipeline_extensions_reads && word == 0x700, "FCALLD owns one target literal"); ++m_pipeline_extensions_reads; } else if (at == caller + 0x603 && layout == 1) { expect(!m_pipeline_operand_reads && word == 0x1234, "The complete two-word delay owns its literal"); ++m_pipeline_operand_reads; } else if (at == caller + 0x604) { expect(!m_pipeline_drained_fetches, "Post-delay continuation is fetched once after far return"); ++m_pipeline_drained_fetches; } });
	m_vector_prefetch_tap = program.install_read_tap(target + 0x700, target + 0x700, "delayed_far_call_target",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) { expect(!m_pipeline_register_writes, "Delayed far target is fetched once"); ++m_pipeline_register_writes; } });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 253670;
	m_cpu->set_state_int(STATE_GENPC, caller + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

std::array<u64,3> tms320c54x_test_state::square_repeat_step(poly_fixture const &f, std::array<u64,3> state, unsigned ordinal)
{
	unsigned const destination = f.operation & 1;
	s64 const old = state[destination] & 0x8000000000ULL ? s64(state[destination]) - 0x10000000000LL : s64(state[destination]);
	s64 const sample = s16(square_repeat_memory(f, ordinal));
	s64 product = sample * (f.tmac ? s16(f.t) : sample) * (f.st1 & 0x40 ? 2 : 1);
	if (f.tmac && (f.pmst & 2) && (f.st1 & 0x240) == 0x240) product = std::clamp(product,s64(-2147483648LL),s64(2147483647LL));
	s64 result = old + (f.operation & (f.tmac ? 4 : 2) ? -product : product);
	if (f.tmac && (f.operation & 2)) result = (result + 32768) & ~s64(65535);
	if (result < -2147483648LL || result > 2147483647LL)
	{
		state[2] |= destination ? 0x200 : 0x400;
		if (f.st1 & 0x200) result = result < 0 ? -2147483648LL : 2147483647LL;
	}
	state[destination] = u64(result) & 0xffffffffffULL; return state;
}

tms320c54x_test_state::delayed_conditional_call_fixture tms320c54x_test_state::delayed_conditional_call_case(unsigned v, bool tc)
{
	delayed_conditional_call_fixture f{};
	if(tc)
	{
		f.tc=true; f.condition=v%2; f.initial_tc=(v/2)%2; f.b=(v/32)%2; f.layout=(v/64)%2;
		f.base=(v/128)%2 ? 0x20000 : 0; f.checkpoint=std::array<unsigned,5>{3,6,7,10,13}[v/256];
		f.initial=std::array<u64,8>{0,1,0x8000000000ULL,0xffffffffffULL,0x7fffffffffULL,0x0100000000ULL,0x0080000000ULL,0x00ffffffffULL}[(v/4)%8];
		f.taken=bool(f.condition)==f.initial_tc; f.changed=f.initial;
		return f;
	}
	f.condition = v % 6; f.b = (v / 48) % 2; f.layout = (v / 96) % 2;
	f.base = (v / 192) % 2 ? 0x20000 : 0;
	f.checkpoint = std::array<unsigned, 5>{3, 6, 7, 10, 13}[v / 384];
	f.initial = std::array<u64, 8>{0, 1, 0x8000000000ULL, 0xffffffffffULL, 0x7fffffffffULL, 0x0100000000ULL, 0x0080000000ULL, 0x00ffffffffULL}[(v / 6) % 8];
	s64 const signed_value = f.initial & 0x8000000000ULL ? s64(f.initial) - 0x10000000000LL : s64(f.initial);
	f.taken = std::array<bool, 6>{signed_value >= 0, signed_value < 0, signed_value != 0, signed_value == 0, signed_value > 0, signed_value <= 0}[f.condition];
	f.literal = f.taken ? std::array<u16, 6>{0xffff, 0, 0, 1, 0, 1}[f.condition] : std::array<u16, 6>{0, 0xffff, 1, 0, 1, 0}[f.condition];
	f.changed = f.layout ? u64(s64(s16(f.literal))) & 0xffffffffffULL : f.initial ^ 0xffffffffffULL;
	return f;
}

void tms320c54x_test_state::start_delayed_conditional_call(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = delayed_conditional_call_case(variant, profile_is("tc-delay-call")); auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) { program.write_word(f.base + 0x600 + i, 0xf495); program.write_word(f.base + 0x700 + i, 0xf495); }
	program.write_word(f.base + 0x600, f.opcode()); program.write_word(f.base + 0x601, 0x700);
	program.write_word(f.base + 0x602, f.delay(0));
	program.write_word(f.base + 0x603, f.delay(1));
	program.write_word(f.base + 0x604, 0x6d94); program.write_word(f.base + 0x605, 0xf4e1);
	program.write_word(f.base + 0x700, 0x6d96); program.write_word(f.base + 0x701, 0xfc00);
	m_cpu->set_pipeline_frontend(false);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.status()); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x900);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000); m_cpu->set_state_int(tms320c54x_device::STATE_RTN, 0x5555);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.b ? 0x123456789aULL : f.initial); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b ? f.initial : 0xfedcba9876ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x1400); m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x1500); m_cpu->set_state_int(tms320c54x_device::STATE_AR6, 0x1600);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = m_fast_stack_reads = m_pipeline_drained_fetches = 0;
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "ccd_taken_frame", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		expect(f.taken && !m_pipeline_store_writes && at == 0x2fff && word == 0x604 && m_cpu->pipeline_cycles() == 7, "Only taken CCD writes one PC+4 frame at the retained Execute boundary"); ++m_pipeline_store_writes; });
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "ccd_return_frame", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		expect(f.taken && !m_pipeline_operand_reads && m_pipeline_store_writes == 1 && at == 0x2fff && word == 0x604, "CCD callee returns through the original retained frame once"); ++m_pipeline_operand_reads;
		auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x2fff, 0xdead); });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x603, "ccd_literal_and_delay", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at == f.base + 0x601) { expect(!m_pipeline_extensions_reads && word == 0x700, "CCD retains one target literal"); ++m_pipeline_extensions_reads; return; }
		unsigned const i = m_fast_stack_reads++;
		u16 const expected = f.delay(i);
		expect(i < 2 && at == f.base + 0x602 + i && word == expected, "CCD fetches each delay word once on both paths"); });
	m_operand_data_tap = program.install_read_tap(f.base + 0x700, f.base + 0x700, "ccd_selected_target", [this, f](offs_t, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		expect(f.taken && !m_pipeline_drained_fetches && word == 0x6d96, "Only taken CCD reaches the callee once"); ++m_pipeline_drained_fetches; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 255420;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::block_conditional_call_fixture tms320c54x_test_state::block_conditional_call_case(unsigned v)
{
	return {std::array<unsigned, 4>{0, 1, 2, 7}[v % 4], std::array<unsigned, 4>{4, 12, 13, 18}[v / 256],
		bool((v / 4) % 2), bool((v / 8) % 2), bool((v / 16) % 2), bool((v / 32) % 2), bool((v / 64) % 2), (v / 128) % 2 ? 0x20000U : 0U};
}

void tms320c54x_test_state::start_block_conditional_call(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = block_conditional_call_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) { program.write_word(f.base + 0x600 + i, 0xf495); program.write_word(f.base + 0x700 + i, 0xf495); }
	program.write_word(f.base + 0x600, 0xf272); program.write_word(f.base + 0x601, f.end());
	program.write_word(f.base + 0x604, (f.b ? 0xe900 : 0xe800) | unsigned(!f.zero)); program.write_word(f.base + 0x605, 0xe589);
	program.write_word(f.base + 0x606, (f.equal ? 0xfb45 : 0xfb44) + (f.b ? 8 : 0)); program.write_word(f.base + 0x607, 0x700);
	program.write_word(f.base + 0x608, f.long_delay ? (f.b ? 0xf120 : 0xf020) : (f.b ? 0xe900 : 0xe800) | unsigned(f.zero));
	program.write_word(f.base + 0x609, f.long_delay ? unsigned(f.zero) : 0x6d95);
	if (!f.endpoint) program.write_word(f.base + 0x60a, 0x6d94);
	program.write_word(f.base + f.end() + 1, 0x6d97); program.write_word(f.base + f.end() + 2, 0xf4e1);
	program.write_word(f.base + 0x700, 0x6d96); program.write_word(f.base + 0x701, 0xfc00);
	for (unsigned i = 0; i <= f.count; ++i) { data.write_word(0x5000 + i, 0x1357 ^ (i * 0x2345)); data.write_word(0x6000 + i, 0xdead); }
	m_cpu->set_pipeline_frontend(false);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_ST0, 0xa608}, {cpu_regs::STATE_ST1, 0x900}, {cpu_regs::STATE_IFR, 0},
		{cpu_regs::STATE_IMR, 0}, {cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_BRC, f.count},
		{cpu_regs::STATE_SP, 0x3000}, {cpu_regs::STATE_RTN, 0x5555}, {cpu_regs::STATE_A, f.b ? 0x123456789aULL : 0},
		{cpu_regs::STATE_B, f.b ? 0 : 0xfedcba9876ULL}, {cpu_regs::STATE_AR2, 0x5000}, {cpu_regs::STATE_AR3, 0x6000},
	});
	for (unsigned r = 4; r < 8; ++r) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + r, 0x1000 + r * 0x100);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_fast_stack_reads = m_pipeline_register_writes = m_pipeline_extensions_reads = m_pipeline_drained_fetches = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "block_ccd_reads", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at == 0x2fff) { expect(f.taken() && m_fast_stack_reads < m_pipeline_register_writes && word == 0x60a, "Block CCD callee reads the original PC+4 frame once per taken call"); ++m_fast_stack_reads; auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x2fff, 0xdead); return; }
		unsigned const i = m_pipeline_operand_reads++; expect(i <= f.count && at == 0x5000 + i && word == u16(0x1357 ^ (i * 0x2345)), "Block CCD preserves each ordered body read across calls"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "block_ccd_writes", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at == 0x2fff) { expect(f.taken() && m_pipeline_register_writes <= f.count && word == 0x60a && m_pipeline_register_writes == m_fast_stack_reads, "Block CCD stacks PC+4, not a synthesized RSA continuation"); ++m_pipeline_register_writes; return; }
		unsigned const i = m_pipeline_store_writes++; expect(i <= f.count && at == 0x6000 + i && word == u16(0x1357 ^ (i * 0x2345)), "Block CCD preserves each ordered body write across calls"); });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x607, f.base + 0x607, "block_ccd_target_literal", [this, f](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(m_pipeline_extensions_reads <= f.count && word == 0x700, "Each block iteration fetches the same CCD target once"); ++m_pipeline_extensions_reads; });
	m_operand_data_tap = program.install_read_tap(f.base + 0x700, f.base + 0x700, "block_ccd_target", [this, f](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(f.taken() && m_pipeline_drained_fetches <= f.count && word == 0x6d96, "Only taken block CCD reaches its callee"); ++m_pipeline_drained_fetches; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 255460;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
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

void tms320c54x_test_state::start_repeat_rearm(unsigned variant)
{
	bool const edge = profile_is("rearm-edge");
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const first = std::array<unsigned, 4>{0, 1, 3, 7}[variant % 4], second = (variant / 4) % 2 ? 2 : 0;
	u32 const base = (edge ? variant % 2 : (variant / 8) % 2) ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	std::array<u16, 14> const words{0xf072, 0x604, 0x6d92, 0xf495, 0xf495, 0x771a, u16(second), 0xf072, 0x60b, 0x6d93, 0xf495, 0xf495, 0x6d94, 0xf4e1};
	for (unsigned i = 0; i < words.size(); ++i) program.write_word(base + 0x600 + i, words[i]);
	if (edge) { program.write_word(base + 0x601, 0x60b); program.write_word(base + 0x602, 0x771a); program.write_word(base + 0x603, 0); }
	m_cpu->set_pipeline_frontend(false);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_ST0, 0xa608}, {cpu_regs::STATE_ST1, 0x800}, {cpu_regs::STATE_IFR, 0},
		{cpu_regs::STATE_IMR, 0}, {cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_BRC, edge ? 3 : first},
		{cpu_regs::STATE_AR2, 0x1200}, {cpu_regs::STATE_AR3, 0x1300}, {cpu_regs::STATE_AR4, 0x1400},
	});
	m_pipeline_store_writes = 0; m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "repeat_rearm_no_reads", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Repeat rearm does not read data memory"); });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "repeat_rearm_write", [this, second, edge](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!edge && at == 0x1a && word == second && !m_pipeline_store_writes, "Only the post-loop STM publishes the next BRC once"); ++m_pipeline_store_writes; });
	m_phase = edge ? 255540 : 255520; m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

bool tms320c54x_test_state::check_repeated_writa_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == repeated_writa_phase)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const standard_width = profile_is("rpt-writa16"); auto const f = repeated_writa_case(variant, standard_width);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Short-source WRITA accepts immediate and memory repeat setup");
		if (f.setup >= 2 && m_port_reads && !m_pipeline_register_writes)
		{
			auto const disable = machine().disable_side_effects();
			if (f.setup == 4) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ((f.reg + 1) % 8), f.count ^ 0xffff);
			else m_cpu->space(AS_DATA).write_word(f.count_address, f.count ^ 0xffff);
			m_pipeline_register_writes = 1;
		}
		if (tick == f.read) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.source, 0xdead); }
		if (tick == f.read + 1) m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x0056789a);
		capture_pipeline_at(f.checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_port_reads, &m_pipeline_register_writes},
			"Save WRITA before PAR capture, at first write or during stream");
		if (tick == f.write + f.count + 24)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(tms320c54x_device::STATE_RC) == 0 && m_pipeline_operand_reads == f.count + 1 && m_pipeline_store_writes == f.count + 1 && m_port_reads == unsigned(f.setup >= 2) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0056789a && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x81abcdef01ULL && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa824 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x900 | (f.form == 1 ? 0x4000 : 0)) &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1200 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5aa5 && (f.form < 2 || m_cpu->state_int(tms320c54x_device::STATE_AR0 + f.reg) == u16(f.source + f.direction * int(f.count + 1))), "WRITA owns count-plus-one transfers and pointer progression without updating A or unrelated state");
			{ auto const disable = machine().disable_side_effects(); for (unsigned n = 0; n <= f.count; ++n) expect(m_cpu->space(AS_PROGRAM).read_word(repeated_writa_destination(f, n)) == repeated_writa_word(f.reg, n), "Repeated WRITA publishes actual program RAM, not just write notifications"); }
			if (replay_pipeline_at(repeated_writa_phase + 1, f.checkpoint,
				"Restore repeated WRITA without duplicate first writes or lost PAR progression")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (standard_width ? 19199 : 38399)) { start_repeated_writa(variant + 1); return true; }
			if (standard_width)
				osd_printf_info("TMS320C54x repeated WRITA: PASS cases=19200 restore=19200 sources=DP,SP,plain,postdec,postinc count_sources=short,long,direct,absolute_RAM,absolute_AR registers=8 counts=0,1,7,255 targets=ordinary,halfspace_cross,wrap16 address_bits=16 ovly=ignored pages=1 snapshots=4 retained_first_source=1 retained_par=1 count_reads=once data_reads=count+1 program_writes=count+1 stream_clocks=1 silicon_phase_claim=0\n");
			else
				osd_printf_info("TMS320C54x repeated WRITA: PASS cases=38400 restore=38400 sources=DP,SP,plain,postdec,postinc count_sources=short,long,direct,absolute_RAM,absolute_AR registers=8 counts=0,1,7,255 targets=ordinary,overlay_cross,wrap23 address_bits=23 ovly=2 pages=2 snapshots=4 retained_first_source=1 retained_par=1 count_reads=once data_reads=count+1 program_writes=count+1 stream_clocks=1 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated repeated WRITA complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == repeated_writa_phase + 1) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = repeated_writa_phase; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_rptz_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == rptz_phase)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, target = variant % 2;
		unsigned const count = std::array<unsigned, 4>{0, 1, 3, 17}[(variant / 2) % 4], checkpoint = std::array<unsigned, 3>{3, 5, 7}[variant / 256];
		u32 const base = (variant / 128) % 2 ? 0x20000 : 0;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Staged RPTZ setup and immediate-store body are admitted");
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (tick >= 7 && !target ? 0 : 0xff12345678ULL) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (tick >= 7 && target ? 0 : 0x81abcdef01ULL) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0xa024 | (((variant / 8) % 4) << 11)) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x800 | (((variant / 32) % 4) << 8)), "RPTZ clears only its selected 40-bit accumulator at Execute and preserves status");
		if (tick == 5) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(base + 0x9001, 0xffff); }
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot[0] = m_pipeline_extensions_reads; m_recurring_irq_snapshot[1] = m_pipeline_store_writes; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save RPTZ before or after count retention and clearing");
		}
		if (tick == 64)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(tms320c54x_device::STATE_RC) == 0 && m_pipeline_extensions_reads == 1 &&
				m_pipeline_store_writes == count + 1 && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x1200 + count + 1, "RPTZ uses retained count-plus-one and reaches its continuation once");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_extensions_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; return restore_pipeline_at(rptz_phase + 1, "Restore staged RPTZ setup without count refetch");
			}
			m_pipeline_fetch_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 767) { start_rptz(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged RPTZ: PASS cases=768 restore=768 accumulators=2 counts=0,1,3,17 c_tc=4 sxm_ovm=4 pages=2 snapshots=3 count_plus_one=1 literal_reads=1 status_preserved=1 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated staged RPTZ complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == rptz_phase + 1) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = rptz_phase; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_braf_control_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 82000 && m_phase < 82004)
	{
		unsigned const index = m_phase - 82000;
		expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "BRAF control fixture terminates");
		expect(m_cpu->state_int(tms320c54x_device::STATE_AR1) == (index == 3 ? 4 : 1) &&
			m_cpu->state_int(tms320c54x_device::STATE_BRC) == (index == 3 ? 0 : 3) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == (index == 2 ? 0x1001 : 0x1000) &&
			!(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x8000),
			"Architectural BRAF controls repeat cancellation and software loop restoration");
		if (index < 3) { start_braf_control_case(index + 1); return true; }
		osd_printf_info("TMS320C54x BRAF control: PASS cases=4 clear=RSBX,STM,POPM restored_loop=1\n");
		start_repeat_endpoint_case(0); return true;
	}
	if (m_phase >= 83000 && m_phase < 83012)
	{
		unsigned const index = m_phase - 83000;
		u16 const body = index % 3 == 0 ? 0x0802 : index % 3 == 1 ? 0xfffd : 0xfffe;
		osd_printf_info("C54x repeat endpoint case=%u body=%04x final_word=%04x continuation=%04x\n",
			index, body, data.read_word(0x0602), data.read_word(0x0fff));
		expect(m_repeat_boundary_requests == 1 && m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Repeat endpoint fixture reaches interrupt vector");
		expect(data.read_word(0x0600) == 0x1234 && data.read_word(0x0601) == 0x1234 &&
			data.read_word(0x0602) == 0x1234 && data.read_word(0x0603) == 0xdead &&
			data.read_word(0x0fff) == u16(body + 2) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0fff &&
			m_cpu->state_int(STATE_GENPC) == (index >= 6 ? 0x010005 : 0x010041),
			"Repeat final iteration retains address progression and IRQ/NMI protection at every endpoint");
		m_repeat_boundary_tap.remove();
		m_cpu->set_input_line(index >= 6 ? INPUT_LINE_NMI : 0, CLEAR_LINE);
		if (index < 11) { start_repeat_endpoint_case(index + 1); return true; }
		osd_printf_info("TMS320C54x repeat endpoints: PASS cases=12 endpoints=ordinary,ffff,0000 counts=short,long sources=irq,nmi\n");
		start_nested_repeat_case(0); return true;
	}
	if (m_phase >= 84000 && m_phase < 84004)
	{
		unsigned const index = m_phase - 84000;
		osd_printf_info("C54x nested repeat case=%u ar1=%04x brc=%04x st1=%04x\n", index,
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR1)),
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_BRC)),
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)));
		expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Nested repeat fixture terminates");
		expect(m_cpu->state_int(tms320c54x_device::STATE_AR1) == (index & 1 ? 12 : 4) &&
			m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0 &&
			!(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x8000),
			"Single repeat at block endpoint completes both architectural loop counts");
		if (index < 3) { start_nested_repeat_case(index + 1); return true; }
		osd_printf_info("TMS320C54x nested repeats: PASS cases=4 counts=zero,two forms=short,long\n");
		start_loop_xc_case(0); return true;
	}
	if (m_phase >= 85000 && m_phase < 85004)
	{
		unsigned const index = m_phase - 85000;
		auto &data = m_cpu->space(AS_DATA);
		osd_printf_info("C54x loop XC case=%u entry_a=%04x continuation=%04x\n", index,
			unsigned(m_irq_accumulator), data.read_word(0x0fff));
		expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Loop XC fixture reaches ISR");
		expect(m_irq_accumulator == (index & 1 ? 1 : 9) &&
			data.read_word(0x0fff) == (index & 1 ? 0x0802 : 0x0803),
			"XC word protection survives block-loop redirection and expires after two words");
		m_xc_irq_tap.remove();
		m_cpu->set_input_line(index >= 2 ? INPUT_LINE_NMI : 2, CLEAR_LINE);
		if (index < 3) { start_loop_xc_case(index + 1); return true; }
		osd_printf_info("TMS320C54x loop XC protection: PASS cases=4 body_words=1,2 sources=irq,nmi\n");
		start_operand_fetch_case(0); return true;
	}
	if (m_phase >= 86000 && m_phase < 86053)
	{
		unsigned const index = m_phase - 86000;
		unsigned const family = index >= 45 ? 10 : index >= 25 ? 6 + (index - 25) / 4 : index / 4;
		unsigned const form = index == 45 ? 4 : index >= 46 ? 3 : index >= 25 ? (index - 25) % 4 : index % 4;
		osd_printf_info("C54x operand fetch case=%u extensions=%u reads=%u early_read=%u\n",
			index, m_operand_extension_count, m_operand_reads, m_operand_order_violation);
		m_operand_fetch_tap.remove(); m_operand_data_tap.remove();
		m_operand_io_tap.remove(); m_operand_program_tap.remove();
		expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Operand fetch fixture terminates");
		expect(!m_operand_order_violation && m_operand_reads == (index == 24 ? 3 : 1),
			"Instruction operand words are fetched before data, I/O or program operand reads");
		auto &data = m_cpu->space(AS_DATA);
		if (index == 24)
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == 0x189ed0000ULL &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0, "FIRS operand data preserved");
		else
		{
			if (family == 0) expect(data.read_word(0x0700) == 0xbeef, "MVDK operand data preserved");
			if (family == 1) expect(m_cpu->space(AS_PROGRAM).read_word(0x0700) == 0xbeef, "MVDP operand data preserved");
			if (family == 2) expect(m_port_writes == 1 && m_first_port_value == 0xbeef, "PORTW operand data preserved");
			if (family >= 3) expect(data.read_word(0x0600) == (family == 3 ? 0xabcd : 0xbeef), "Input operand data preserved");
			if (family == 6 || family == 7) expect(m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x1000, "Comparison/test publishes TC");
			if (family == 8) expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0701 &&
				data.read_word(0x0700) == 0xbeef, "POPD increments SP once and preserves popped stack contents");
			if (family == 9) expect(m_cpu->space(AS_PROGRAM).read_word(0x0900) == 0xbeef &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x0900, "WRITA writes the distinct program destination without changing A");
			if (family == 10) expect(data.read_word(0x05ff) == 0xcafe &&
				m_cpu->space(AS_PROGRAM).read_word(0x0900) == 0xbeef, "MVPD preserves the preceding destination and program source");
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR2) ==
				(form == 1 ? 0x0601 : form == 2 ? 0x05fb : 0x0600), "Address modifier preserved");
		}
		if (index < 52) { start_operand_fetch_case(index + 1); return true; }
		osd_printf_info("TMS320C54x operand fetch order: PASS cases=53 families=12 forms=direct,indirect,long,absolute,preincrement\n");
		start_cala_rtn_case(0); return true;
	}
	if (m_phase >= 87000 && m_phase < 87004)
	{
		unsigned const index = (m_phase - 87000) / 2;
		auto &data = m_cpu->space(AS_DATA);
		if (!(m_phase & 1))
		{
			expect(m_cpu->state_int(STATE_GENPC) == 0x0901 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x03ff &&
				data.read_word(0x03ff) == 0x0801, "CALA stacks its continuation");
			data.write_word(0x03ff, 0xdead);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			++m_phase;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(STATE_GENPC) == 0x0802 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0400 &&
			data.read_word(0x03ff) == 0xdead, "CALA publishes RTN independently of its stack word");
		if (!index) { start_cala_rtn_case(1); return true; }
		osd_printf_info("TMS320C54x accumulator call fast return: PASS cases=2 sources=A,B\n");
		start_instruction_word_case(0); return true;
	}
	if (m_phase >= 88000 && m_phase < 88000 + instruction_word_examples().size())
	{
		unsigned const index = m_phase - 88000;
		auto const &test = instruction_word_examples().at(index);
		check_case_state(*m_cpu, test);
		expect_opcode(test.opcode, true, "Literal atomic instruction reference");
		if (index + 1 < instruction_word_examples().size())
		{
			start_instruction_word_case(index + 1);
			return true;
		}
		osd_printf_info("TMS320C54x atomic instruction references: PASS cases=%u resets=%u firmware=0\n",
			unsigned(instruction_word_examples().size()), unsigned(instruction_word_examples().size()));
		start_indirect_popm_case(0); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_block_page_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 294000)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = block_page_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Cross-page repeat hazard uses retained ordinary instructions and a balanced far call");
		capture_pipeline_at(f.checkpoint, {&m_pipeline_store_writes, &m_pipeline_operand_reads},
			"Save before, during or after the foreign-page repeat endpoint");
		if (tick == 240)
		{
			unsigned const calls = f.hazard ? 1 : f.count + 1;
			if (m_cpu->state_int(tms320c54x_device::STATE_AR4) != 0x1400 + calls || m_cpu->state_int(tms320c54x_device::STATE_AR5) != 0x1500 + (f.hazard ? f.count : 0))
				osd_printf_info("block_page_mismatch: case=%u staged=%u hazard=%u count=%u caller=%06x callee=%06x ar4=%04x ar5=%04x ar6=%04x pc=%06x brc=%04x st1=%04x stack_writes=%u stack_reads=%u\n", variant, unsigned(f.staged), unsigned(f.hazard), f.count, unsigned(f.caller), unsigned(f.callee), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR4)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR5)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR6)), unsigned(m_cpu->state_int(STATE_GENPC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_BRC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)), m_pipeline_store_writes, m_pipeline_operand_reads);
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.caller + 0x60b && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x1400 + calls && m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x1500 + (f.hazard ? f.count : 0) && m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x1600 + f.count + 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0x1700 + (f.delayed ? calls : 0) &&
				m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_RSA) == 0x602 && m_cpu->state_int(tms320c54x_device::STATE_REA) == 0x609 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x900 &&
				m_pipeline_store_writes == 2 * calls && m_pipeline_operand_reads == 2 * calls,
				"REA compares only the low PC; a foreign-page match consumes the loop and wraps within that foreign page");
			if (replay_pipeline_at(294010, f.checkpoint,
				"Restore the actual page, repeat count and pending call frame without duplicating work")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 383) { start_block_page(variant + 1); return true; }
			osd_printf_info("TMS320C54x repeat page comparator: PASS cases=384 restore=384 frontends=atomic,staged counts=0,1,2,7 caller_pages=0,2,127 targets=foreign call_forms=FCALL,FCALLD hazard_controls=2 snapshots=4 rea_width=16 wrap_page=current balanced_frames=1 delay_effects=once physical_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated repeat page comparator complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 294010) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 294000; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_branch_repeat_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 600060)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		auto const f = branch_repeat_case(variant);
		unsigned const end = f.target == 0x500 ? 0x604 : 0x605;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "RPTBD delay repeat is separate from its outer block");
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save RPTBD delay and short-repeat state");
		}
		if (tick == f.limit)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + end + 2 &&
				m_pipeline_operand_reads == (f.kind == 1 ? f.count + 1 : 0) &&
				m_pipeline_store_writes == (f.kind == 2 ? 0 : f.count + 1) && m_pipeline_extensions_reads == f.following + 1,
				"Delay body executes count+1 once before the outer block executes BRC+1 times");
			if (m_cpu->state_int(tms320c54x_device::STATE_ST1) != f.status)
				osd_printf_info("RPTBD delay state mismatch case=%u staged=%u replay=%u st1=%04x expected=%04x\n", variant, unsigned(f.staged), unsigned(m_pipeline_replay), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)), f.status);
			expect(m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_RC) == 0 &&
				m_cpu->state_int(tms320c54x_device::STATE_RSA) == 0x604 && m_cpu->state_int(tms320c54x_device::STATE_REA) == end &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.status &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x2001 + f.following &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x1800 + (f.kind == 2 ? 0 : f.count + 1) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x1c00 + (f.kind == 1 ? f.count + 1 : 0),
				"Both loop controllers and their pointers settle independently");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2];
				m_check_timer->adjust(attotime::never);
				m_cpu->set_pipeline_frontend(f.staged);
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Restore RPTBD delay repeat after clearing the live pipeline");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 600061; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 5759) { start_block_delay_repeat(variant + 1); return true; }
			osd_printf_info("TMS320C54x RPTBD delay repeat: PASS cases=5760 restore=5760 frontends=2 bodies=3 inner_counts=0,1,8,39,255 outer_counts=0,2,7 block_layouts=2 patterns=4 pages=2 snapshots=4 delay_effects=once reset_before_restore=1 physical_phase_claim=0\n");
			throw emu_fatalerror(3, "TMS320C54x isolated RPTBD delay repeat complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 600061) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 600060; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 600050)
	{
		bool const block = profile_is("rptbd-rpt-edge");
		bool const conditional = profile_is("bcd-rpt-edge");
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, checkpoint = variant < 16 ? 3 : 10;
		unsigned const kind = variant%8;
		expect(m_pipeline_operand_reads <= unsigned(kind == 3) && !m_pipeline_store_writes,"Unqualified delay repeats perform no transaction beyond their owner's preceding return Read");
		if (tick == checkpoint && !m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_operand_reads,m_pipeline_store_writes}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE,"Save rejected delay-repeat state before or after the guard"); }
		if (tick == 24)
		{
			bool const state_ok = m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (block ? 0x8800 : 0x800) && m_cpu->state_int(tms320c54x_device::STATE_RC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 13 && m_cpu->state_int(tms320c54x_device::STATE_SP) == (kind == 2 ? 0x2fff : kind == 3 ? 0x3001 : 0x3000) && m_pipeline_operand_reads == unsigned(kind == 3) && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5aa5 && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x1234560000ULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfeabcdffffULL;
			if (!state_ok) osd_printf_info("BD repeat exclusion mismatch case=%u replay=%u illegal=%llu st0=%04x st1=%04x rc=%04x brc=%04x sp=%04x t=%04x a=%010llx b=%010llx\n",variant,unsigned(m_pipeline_replay),(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST0)),unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)),unsigned(m_cpu->state_int(tms320c54x_device::STATE_RC)),unsigned(m_cpu->state_int(tms320c54x_device::STATE_BRC)),unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)),unsigned(m_cpu->state_int(tms320c54x_device::STATE_T)),(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B));
			expect(state_ok,"Delay-repeat exclusions preserve counters and arithmetic state while retaining the enclosing control's stack effect");
			for (unsigned ar = 0; ar < 8; ++ar) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0+ar) == (ar == 3 ? 0x2000 : ar == 4 ? 0x1800 : ar == 5 ? 0x1c00 : 0x4000+ar),"Rejected delay repeats preserve every address register");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_check_timer->adjust(attotime::never); m_cpu->set_pipeline_frontend(true); expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE,"Restore unsupported delay repeat without introducing side effects"); m_cpu->suspend(SUSPEND_REASON_DISABLE,true); m_phase = 600051; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (block || conditional)
			{
				unsigned const ordinal = (variant / 8) * 5 + (kind == 0 ? 0 : kind - 3);
				if (conditional) { if (ordinal < 19) { start_conditional_delay_repeat_edge(ordinal + 1); return true; } osd_printf_info("TMS320C54x BCD delay repeat exclusions: PASS cases=20 restore=20 forms=late,long_count,memory_count,store_spill,copy_spill pages=2 snapshots=2 reads=0 writes=0 ar_changes=0 counter_changes=0 reset_before_restore=1 silicon_trap_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated BCD delay repeat exclusions complete"); }
				if (ordinal < 19) { start_block_delay_repeat_edge(ordinal + 1); return true; }
				osd_printf_info("TMS320C54x RPTBD delay repeat exclusions: PASS cases=20 restore=20 forms=late,long_count,memory_count,store_spill,copy_spill pages=2 snapshots=2 reads=0 writes=0 ar_changes=0 repeat_counter_changes=0 owner_setup_retained=1 reset_before_restore=1 silicon_trap_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated RPTBD delay repeat exclusions complete");
			}
			if (variant < 31) { start_branch_repeat_edge(variant+1); return true; }
			osd_printf_info("TMS320C54x BD delay repeat exclusions: PASS cases=32 restore=32 forms=late,BANZD,CALLD_late,RETD,long_count,memory_count,store_spill,copy_spill pages=2 snapshots=2 reads=RETD_stack_only writes=0 ar_changes=0 counter_changes=0 owner_stack=CALLD,RETD reset_before_restore=1 silicon_trap_claim=0\n"); throw emu_fatalerror(3,"TMS320C54x isolated BD delay repeat exclusions complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true;
	}
	if (m_phase == 600051) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 600050; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true; }
	if (m_phase == 600040)
	{
		bool const returning = profile_is("retd-rpt");
		bool const calling = profile_is("calld-rpt");
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = branch_repeat_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),"Short RPT in BD's first delay word has independent branch and repeat ownership");
		if (!returning && tick == 5) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base+0x601,0x620); }
		if (returning && tick == 8) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x2fff,0x620); }
		if (f.conditional && f.staged && tick == 6) { m_cpu->set_state_int(tms320c54x_device::STATE_A,f.a(true)); m_cpu->set_state_int(tms320c54x_device::STATE_B,f.b(true)); }
		if (f.staged && !f.conditional && tick == 6) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base+(returning ? 0x602 : 0x603),0xe8ff); }
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads,m_pipeline_store_writes,m_pipeline_extensions_reads}; m_saved_observed_instructions = m_observed_instructions;
			m_recurring_irq_snapshot[3] = m_rptbd_counts.reads;
			m_recurring_irq_snapshot[4] = m_pipeline_register_writes;
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE,"Save delayed branch continuation and repeat before, during or after the delay body");
		}
		if (tick == f.limit)
		{
			if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE) || m_cpu->state_int(STATE_GENPC) != f.base+(calling ? 0x605 : f.continuation()+3))
				osd_printf_info("BD repeat mismatch case=%u staged=%u replay=%u pc=%06x reads=%u old_writes=%u target_writes=%u\n",variant,unsigned(f.staged),unsigned(m_pipeline_replay),unsigned(m_cpu->state_int(STATE_GENPC)),m_pipeline_operand_reads,m_pipeline_store_writes,m_pipeline_extensions_reads);
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base+(calling ? 0x605 : f.continuation()+3) && m_pipeline_operand_reads == (f.kind == 1 ? f.count+1 : 0) && m_pipeline_store_writes == (f.kind == 2 ? 0 : f.count+1) && m_pipeline_extensions_reads == f.following+1,"Delay repeat drains once, then the retained target runs its own complete repeat");
			unsigned bodies = 0, target_bodies = 0, branches = 0, headers = 0;
			if (returning) expect(m_rptbd_counts.reads == 1, "RETD never resamples its poisoned return word");
			if (calling) expect(m_rptbd_counts.reads == 1 && m_pipeline_register_writes == 1, "CALLD and RET own one balanced continuation frame");
			for (auto const &[address,word] : m_observed_instructions)
			{
				if (address == f.base+(returning ? 0x602 : 0x603)) { expect(word == f.body(),"Repeated delay retains its original opcode"); ++bodies; }
				if (address == f.base+f.continuation()+1) ++target_bodies;
				if (address == f.base+0x600) ++branches;
				if (address == f.base+f.continuation()) { expect(bodies == f.count+1,"Target repeat cannot execute before all delay-body copies"); ++headers; }
			}
			expect(bodies == f.count+1 && target_bodies == f.following+1 && branches == 1 && headers == 1,"Execution-owned observations distinguish delay copies from one fresh target sequence");
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x2001+f.following && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x1800+(f.kind == 2 ? 0 : f.count+1) && m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x1c00+(f.kind == 1 ? f.count+1 : 0),"Both repeat lifecycles update their own pointers exactly once per copy");
			expect(m_cpu->state_int(tms320c54x_device::STATE_RC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 13 && m_cpu->state_int(tms320c54x_device::STATE_RSA) == 0x5555 && m_cpu->state_int(tms320c54x_device::STATE_REA) == 0x6666 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5aa5 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.status && m_cpu->state_int(tms320c54x_device::STATE_A) == f.a(true) && m_cpu->state_int(tms320c54x_device::STATE_B) == f.b(true),"Delay repeat preserves unrelated counters, stack, status and accumulator state");
			{
				auto const disable = machine().disable_side_effects(); auto &data = m_cpu->space(AS_DATA);
				for (unsigned i = 0; i < 256; ++i)
				{
					expect(data.read_word(0x1800+i) == (f.kind == 0 && i <= f.count ? u16(f.a(true)) : u16(f.seed+i*257)),"Delay source/store buffer remains exact after replay");
					expect(data.read_word(0x1c00+i) == (f.kind == 1 && i <= f.count ? u16(f.seed+i*257) : 0xdead),"Delay copy buffer has no omitted or extra words");
				}
				for (unsigned i = 0; i < 32; ++i) expect(data.read_word(0x2000+i) == (i <= f.following ? u16(f.b(true)) : 0xdead),"Target repeat has an independent count and untouched tail");
				for (u16 address : {0x17ff,0x1900,0x1bff,0x1d00,0x1fff,0x2020}) expect(data.read_word(address) == 0x2468,"Delayed repeat preserves every buffer guard");
			}
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2]; m_observed_instructions = m_saved_observed_instructions;
				m_rptbd_counts.reads = m_recurring_irq_snapshot[3];
				m_pipeline_register_writes = m_recurring_irq_snapshot[4];
				m_check_timer->adjust(attotime::never); m_cpu->set_pipeline_frontend(f.staged); expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE,"Restore pending delayed target after clearing live pipeline state");
				m_cpu->suspend(SUSPEND_REASON_DISABLE,true); m_phase = 600041; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (f.conditional ? 69119U : 5759U)) { start_branch_repeat(variant+1); return true; }
			if (f.conditional) { osd_printf_info("TMS320C54x BCD delay repeat: PASS cases=69120 restore=69120 frontends=atomic,staged predicates=12 signed_patterns=4 bodies=STL,MVDD,NOP counts=0,1,8,39,255 path_counts=0,2,7 directions=2 pages=2 snapshots=4 taken_fallthrough=1 predicate_poison=staged_after_Read target_poison=1 opcode_poison=staged_only ordered_copies=1 fresh_path_repeat=1 reset_before_restore=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated BCD delay repeat complete"); }
			if (calling) { osd_printf_info("TMS320C54x CALLD delay repeat: PASS cases=5760 restore=5760 frontends=atomic,staged bodies=STL,MVDD,NOP counts=0,1,8,39,255 target_counts=0,2,7 directions=2 patterns=4 pages=2 snapshots=4 stack_reads=1 stack_writes=1 balanced_stack=1 target_poison=1 ordered_copies=1 fresh_target_repeat=1 opcode_poison=staged_only reset_before_restore=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated CALLD delay repeat complete"); }
			if (returning)
			{
				osd_printf_info("TMS320C54x RETD delay repeat: PASS cases=5760 restore=5760 frontends=atomic,staged bodies=STL,MVDD,NOP counts=0,1,8,39,255 target_counts=0,2,7 directions=2 patterns=4 pages=2 snapshots=4 stack_reads=1 balanced_stack=1 target_poison=1 ordered_copies=1 fresh_target_repeat=1 opcode_poison=staged_only reset_before_restore=1 physical_phase_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated RETD delay repeat complete");
			}
			osd_printf_info("TMS320C54x BD delay repeat: PASS cases=5760 restore=5760 frontends=atomic,staged bodies=STL,MVDD,NOP counts=0,1,8,39,255 target_counts=0,2,7 directions=2 patterns=4 pages=2 snapshots=4 ordered_copies=1 target_retained=1 fresh_target_repeat=1 opcode_poison=staged_only reset_before_restore=1 physical_phase_claim=0\n"); throw emu_fatalerror(3,"TMS320C54x isolated BD delay repeat complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true;
	}
	if (m_phase == 600041) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 600040; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_pipeline_repeat_nop_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 252750)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const long_form = variant % 2;
		unsigned const count = std::array<unsigned, 5>{0, 1, 3, 32, 255}[(variant / 2) % 5], interrupt = (variant / 10) % 3;
		unsigned const first = long_form ? 8 : 7, last = first + count;
		u32 const base = (variant / 30) % 2 ? 0x20000 : 0, body = base + (long_form ? 0x602 : 0x601);
		unsigned const checkpoint = std::array<unsigned, 4>{3, first, first + count / 2, last}[variant / 60];
		unsigned const remaining = tick < first - 1 ? 0 : tick <= first ? count : tick <= last ? last - tick : 0;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_RC) == remaining,
			"RPT count loads once, first NOP retains count, and each further NOP consumes one saved counter clock");
		// Input-line changes synchronize through MAME. Queue before Decode
		// so the cycle-3 checkpoint saves a latched event, not an unsaved queue.
		if (interrupt && tick == 2) { int const line = interrupt == 2 ? INPUT_LINE_NMI : 0; m_cpu->set_input_line(line, ASSERT_LINE); m_cpu->set_input_line(line, CLEAR_LINE); }
		if (interrupt == 1 && tick == 3) expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) & 1, "Repeat Decode checkpoint owns a delivered pending IRQ");
		if (tick < last) expect(!m_vector_acks && !m_pipeline_store_writes, "RPT suppresses IRQ and NMI until its count+1 NOP body completes");
		if (tick == (long_form ? 4 : 3))
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_PROGRAM).write_word(body, 0xe811);
		}
		if (tick == (long_form ? 5 : 4))
		{
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(body + 1, 0xe866);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads, m_vector_acks};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save repeat Decode, first/middle/final NOP with pending interrupt");
		}
		if (tick == last + 30)
		{
			bool const success = m_pipeline_operand_reads == 1 && m_pipeline_extensions_reads == (interrupt && !count ? 3 : 2) &&
				m_pipeline_store_writes == (interrupt ? 2 : 0) && m_vector_acks == unsigned(bool(interrupt)) &&
				m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x66 &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == 0x1234 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000;
			if (!success) osd_printf_info("Staged RPT NOP failure: case=%u replay=%u count=%u interrupt=%u cycle=%llu idle=%u a=%010llx b=%010llx sp=%04x body_reads=%u continuation_reads=%u stack_writes=%u acks=%u\n",
				variant, m_pipeline_replay, count, interrupt, (unsigned long long)m_cpu->pipeline_cycles(), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IDLE)),
				(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)), m_pipeline_operand_reads, m_pipeline_extensions_reads, m_pipeline_store_writes, m_vector_acks);
			expect(success,
				"Repeated NOP preserves its body, completes exact duration and returns any deferred interrupt to its fresh continuation");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_operand_reads = m_recurring_irq_snapshot[0];
				m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2]; m_vector_acks = m_recurring_irq_snapshot[3];
				return restore_pipeline_at(252760, "Restore repeat counter/body/interrupt without restarting the loop");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 239) { start_pipeline_repeat_nop(variant + 1); return true; }
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			osd_printf_info("TMS320C54x staged repeat NOP: PASS cases=240 forms=short,long counts=0,1,3,32,255 sources=none,IRQ,NMI pages=2 restore=240 count_plus_one=1 counter_each_clock=1 duplicate_body_fetches=0 deferred_interrupts=1\n");
			start_pipeline_xmem_bit(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252760)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252750;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_repeat_rearm_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 255540)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, checkpoint = 6 + variant / 2;
		if (tick == checkpoint && !m_pipeline_replay) { expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save actual in-body BRC exclusion before or after its Access guard"); }
		if (tick == 20)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && !m_pipeline_store_writes && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 3 && m_cpu->state_int(tms320c54x_device::STATE_RSA) == 0x602 && m_cpu->state_int(tms320c54x_device::STATE_REA) == 0x60b && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x1200 && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x1300 && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x1400, "Actual in-body STM BRC remains excluded before publication or body effects");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; return restore_pipeline_at(255550, "Restore the in-body guard without enabling a BRC write"); }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 7) { start_repeat_rearm(variant + 1); return true; }
			osd_printf_info("TMS320C54x in-body STM BRC exclusion: PASS cases=8 restore=8 pages=2 snapshots=4 write_owner=Access publications=0 body_effects=0 silicon_trap_claim=0\n");
			throw emu_fatalerror(3, "TMS320C54x isolated in-body STM BRC exclusion complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255550) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255540; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 255520)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		unsigned const first = std::array<unsigned, 4>{0, 1, 3, 7}[variant % 4], second = (variant / 4) % 2 ? 2 : 0;
		u32 const base = (variant / 8) % 2 ? 0x20000 : 0;
		unsigned const checkpoint = 10 + first * 3 + variant / 16;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Post-loop STM must not inherit the older endpoint's body ownership");
		if (tick == checkpoint && !m_pipeline_replay) { m_pipeline_store_case = m_pipeline_store_writes; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save adjacent block rearm across endpoint and STM publication"); }
		if (tick == 100)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == base + 0x60e && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x1201 + first && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x1301 + second && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x1401 && !m_cpu->state_int(tms320c54x_device::STATE_BRC) && m_cpu->state_int(tms320c54x_device::STATE_RSA) == 0x609 && m_cpu->state_int(tms320c54x_device::STATE_REA) == 0x60b && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800 && m_pipeline_store_writes == 1, "Consecutive blocks own separate counts and execute their continuation once");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_store_writes = m_pipeline_store_case; return restore_pipeline_at(255530, "Restore rearm without duplicate BRC publication or iterations"); }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 63) { start_repeat_rearm(variant + 1); return true; }
			osd_printf_info("TMS320C54x adjacent block rearm: PASS cases=64 restore=64 first_counts=0,1,3,7 second_counts=0,2 pages=2 snapshots=4 brc_publications=1 actual_iterations=1 write_owner=Access interrupt_composition=unvalidated physical_phase_claim=0\n");
			throw emu_fatalerror(3, "TMS320C54x isolated adjacent block rearm complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255530) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255520; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_delayed_call_replay_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 255460)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = block_conditional_call_case(variant); unsigned const iterations = f.count + 1, calls = f.taken() ? iterations : 0;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Block CCD and complete delay span remain admitted");
		capture_pipeline_at(f.checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_fast_stack_reads, &m_pipeline_register_writes, &m_pipeline_extensions_reads, &m_pipeline_drained_fetches},
			"Save block CCD before predicate, frame, endpoint or return");
		if (tick == 400)
		{
			if (m_pipeline_operand_reads != iterations || m_pipeline_store_writes != iterations || m_cpu->state_int(tms320c54x_device::STATE_BRC) || m_cpu->state_int(STATE_GENPC) != f.base + f.end() + 3)
				osd_printf_info("Block CCD mismatch: case=%u replay=%u endpoint=%u taken=%u pc=%06x brc=%04x st1=%04x reads=%u writes=%u frames=%u returns=%u calls=%u expected_iterations=%u\n", variant, unsigned(m_pipeline_replay), unsigned(f.endpoint), unsigned(f.taken()), unsigned(m_cpu->state_int(STATE_GENPC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_BRC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)), m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes, m_fast_stack_reads, m_pipeline_drained_fetches, iterations);
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + f.end() + 3 && !m_cpu->state_int(tms320c54x_device::STATE_BRC) && m_cpu->state_int(tms320c54x_device::STATE_RSA) == 0x604 && m_cpu->state_int(tms320c54x_device::STATE_REA) == f.end() &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x900 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 &&
				m_cpu->state_int(f.b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == unsigned(f.zero) && m_cpu->state_int(f.b ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == (f.b ? 0x123456789aULL : 0xfedcba9876ULL) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x5000 + iterations && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x6000 + iterations && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x1400 + (f.endpoint ? 0 : iterations) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x1500 + (f.long_delay ? 0 : iterations) && m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x1600 + calls && m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0x1701 &&
				m_pipeline_operand_reads == iterations && m_pipeline_store_writes == iterations && m_pipeline_extensions_reads == iterations && m_fast_stack_reads == calls && m_pipeline_register_writes == calls && m_pipeline_drained_fetches == calls,
				"Block CCD balances calls and returns through every Decode-owned outer iteration, including endpoint delay spans");
			{ auto const disable = machine().disable_side_effects(); for (unsigned i = 0; i < iterations; ++i) expect(m_cpu->space(AS_DATA).read_word(0x6000 + i) == u16(0x1357 ^ (i * 0x2345)), "Block CCD final memory matches independent copy oracle"); }
			if (replay_pipeline_at(255470, f.checkpoint,
				"Restore block CCD without losing loop state or duplicating transactions")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_operand_data_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 1023) { start_block_conditional_call(variant + 1); return true; }
			osd_printf_info("TMS320C54x delayed conditional calls in block: PASS cases=1024 restore=1024 counts=0,1,2,7 conditions=AEQ,ANEQ,BEQ,BNEQ outcomes=2 positions=middle,endpoint delay_layouts=2 pages=2 snapshots=4 ordered_copies=1 balanced_frames=1 stack_return=PC+4 outer_count=Decode interrupt_composition=unvalidated physical_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated delayed conditional calls in block complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255470) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255460; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 255420)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = delayed_conditional_call_case(variant,profile_is("tc-delay-call"));
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "CCD and complete delay instructions stay in supported forms");
		if (!f.taken) expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000, "Untaken CCD never reserves a stack frame");
		if (tick == 3 || tick == 4 || tick == 6) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + (tick == 3 ? 0x601 : tick == 4 ? 0x602 : 0x603), 0xdead); }
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads, m_fast_stack_reads, m_pipeline_drained_fetches}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save CCD predicate, pending stack write, delay execution or return");
		}
		if (tick == 40)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + 0x606 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 &&
				m_cpu->state_int(tms320c54x_device::STATE_RTN) == (f.taken ? 0x604 : 0x5555) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.status(true) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x900 &&
				m_cpu->state_int(f.b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == f.changed && m_cpu->state_int(f.b ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == (f.b ? 0x123456789aULL : 0xfedcba9876ULL) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x1401 && m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x1500 + unsigned(f.tc || !f.layout) && m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x1600 + unsigned(f.taken) &&
				m_pipeline_operand_reads == unsigned(f.taken) && m_pipeline_store_writes == unsigned(f.taken) && m_pipeline_drained_fetches == unsigned(f.taken) && m_pipeline_extensions_reads == 1 && m_fast_stack_reads == 2,
				"CCD retains the original predicate, runs delay work once on both paths and balances only a taken frame");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2]; m_fast_stack_reads = m_recurring_irq_snapshot[3]; m_pipeline_drained_fetches = m_recurring_irq_snapshot[4];
				m_cpu->set_pipeline_frontend(true); return restore_pipeline_at(255430, "Restore delayed conditional call without duplicate effects or a changed decision");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_operand_data_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (f.tc ? 1279 : 1919)) { start_delayed_conditional_call(variant + 1); return true; }
			if(f.tc) { osd_printf_info("TMS320C54x TC delayed calls: PASS cases=1280 restore=1280 predicates=TC,NTC initial_tc=2 accumulator_patterns=8 seed_accumulators=2 delay_layouts=2 pages=2 snapshots=5 tc_flip=organic_delay target_delay_poison=1 taken_frame=PC+4 untaken_stack_accesses=0 balanced_return=1 reset_before_restore=1 block_interrupt_acceptance=0 physical_phase_claim=0\n"); throw emu_fatalerror(3,"TMS320C54x isolated TC delayed calls complete"); }
			osd_printf_info("TMS320C54x delayed conditional call: PASS cases=1920 restore=1920 conditions=12 patterns=8 delay_layouts=2 pages=2 snapshots=5 target_literals=1 delay_words=2 delay_effects=once predicate_retained=1 taken_frame=PC+4 untaken_stack_accesses=0 balanced_return=1 interrupt_composition=unvalidated physical_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated delayed conditional call complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255430) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255420; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_basic_repeat_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 310440)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		auto const f = basic_repeat_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Basic memory repeat retains an admitted count and body");
		if (tick == (f.absolute ? 5U : 4U)) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.body, 0xe8ff); }
		if (tick == (f.absolute ? 6U : 5U) && f.address >= 0x60) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.address, 0xdead); }
		if (f.irq && tick == 3) m_cpu->set_input_line(f.irq == 2 ? INPUT_LINE_NMI : 2, ASSERT_LINE);
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_rptbd_saved_counts = m_rptbd_counts; m_rptbd_saved_return = m_rptbd_return; m_saved_observed_instructions = m_observed_instructions;
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save memory repeat before count capture, during hold or body");
		}
		if (tick == f.count + 80)
		{
			unsigned iterations = 0; for (auto const &[address, word] : m_observed_instructions) if (address == f.body) { expect(word == 0xf495, "Memory repeat executes the retained body word"); ++iterations; }
			if (iterations != f.count + 1 || m_rptbd_counts.reads != 1 || m_rptbd_counts.fetches != 1 || m_rptbd_counts.acks != unsigned(bool(f.irq)))
				osd_printf_info("Memory repeat mismatch case=%u replay=%u count=%u iterations=%u reads=%u fetches=%u acks=%u stack=%u/%u pc=%06x rc=%04x\n", variant, unsigned(m_pipeline_replay), f.count, iterations, m_rptbd_counts.reads, m_rptbd_counts.fetches, m_rptbd_counts.acks, m_rptbd_counts.stack_reads, m_rptbd_counts.stack_writes, unsigned(m_cpu->state_int(STATE_GENPC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_RC)));
			expect(iterations == f.count + 1 && m_rptbd_counts.reads == 1 && m_rptbd_counts.fetches == 1 &&
				m_rptbd_counts.acks == unsigned(bool(f.irq)) && m_rptbd_counts.stack_reads == unsigned(bool(f.irq)) && m_rptbd_counts.stack_writes == unsigned(bool(f.irq)), "Memory repeat executes count+1 bodies with one count read and a balanced deferred interrupt");
			u16 const st0 = f.compatibility && f.form >= 2 && f.form < 26 && (f.mode & 7) ? (f.st0 & ~0xe000) | ((f.mode & 7) << 13) : f.st0;
			for (unsigned i = 0; i < 8; ++i)
			{
				u16 const ar = f.address == 0x10 + i ? f.count : 0x5000 + (f.form >= 2 && f.form < 26 && i == f.selected ? f.modification == 1 ? -1 : f.modification == 2 ? 1 : 0 : 0);
				expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == ar, "Count addressing modifies its selected pointer once, never once per body");
			}
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + 0x610 && m_cpu->state_int(tms320c54x_device::STATE_RC) == 0 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 13 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == st0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfedcba9876ULL, "Memory repeat preserves flags and unrelated state through its fresh continuation");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_rptbd_counts = m_rptbd_saved_counts; m_rptbd_return = m_rptbd_saved_return; m_observed_instructions = m_saved_observed_instructions;
				return restore_pipeline_at(310450, "Restore sampled count and pointer without resampling or duplicated effects");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 10375) { start_basic_repeat(variant + 1); return true; }
			osd_printf_info("TMS320C54x basic memory repeat: PASS cases=10376 restore=10376 forms=36 counts=0,1,7,255,32768,65535 pages=2 cpl=2 cmpt=2 sources=none,IRQ,NMI snapshots=3 count_reads=1 body_fetches=1 body_count=rc+1 pointer_effect=once retained_count=1 balanced_frames=1 physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated basic memory repeat complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 310450) { m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); m_phase = 310451; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 310451)
	{
		auto const f = basic_repeat_case(m_pipeline_peripheral_case);
		// Restore the fixture's external asserted-until-ack input too:
		// the earliest snapshot can precede queued line synchronization.
		if (f.irq && f.checkpoint == 3) m_cpu->set_input_line(f.irq == 2 ? INPUT_LINE_NMI : 2, ASSERT_LINE);
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 310440; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_rptbd_square_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 62178)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = rptbd_square_case(variant);
		unsigned completed = 0; for (unsigned i = 0; i < f.operations(); ++i) completed += tick > 7 + f.slot(i);
		auto const out = f.result(completed); unsigned const last_read = 7 + f.slot(f.operations() - 1), checkpoint = std::array<unsigned, 4>{5, 7, 8, 9}[f.snapshot];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_A) == out[0] && m_cpu->state_int(tms320c54x_device::STATE_B) == out[1] && m_cpu->state_int(tms320c54x_device::STATE_ST0) == out[2], "RPTBD delay squares retain sequential accumulator, carry and sticky overflow ownership");
		for (unsigned i = 0; i < f.operations(); ++i) if (tick == 7 + f.slot(i)) expect(m_cpu->state_int(tms320c54x_device::STATE_T) == f.memory(i), "Delay square publishes its own T word only at Read");
		if (tick == 6) { auto const disabled = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x602, 0xdead); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x603, 0xbeef); }
		if (tick == last_read - 1) { expect(m_cpu->state_int(tms320c54x_device::STATE_AR5) == (f.form == 2 ? 0x5000 + f.operations() : 0x5000), "RPTBD square addressing updates its pointer once per source"); m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x7000); }
		if (tick == last_read) { auto const disabled = machine().disable_side_effects(); for (unsigned i = 0; i < f.operations(); ++i) m_cpu->space(AS_DATA).write_word(f.address(i), 0xdead); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xdead); }
		if (tick > last_read) expect(m_cpu->state_int(tms320c54x_device::STATE_T) == 0xdead, "Delay square Execute cannot overwrite a newer T value");
		if (tick == checkpoint && !m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes, m_pipeline_extensions_reads}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save RPTBD delay square operands and pending block prediction"); }
		if (tick == 64)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == f.operations() && m_pipeline_extensions_reads == 3 && m_pipeline_store_writes == f.count + 1 && m_pipeline_register_writes == f.count + 1 && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1() && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x6001 + f.count && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x6101 + f.count && m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x7000 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000, "RPTBD delay work executes once before all count-plus-one body writes");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_register_writes = m_recurring_irq_snapshot[2]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[3]; m_check_timer->adjust(attotime::never); m_cpu->set_pipeline_frontend(false); m_cpu->set_pipeline_frontend(true); expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Restore square delay and block state without future pipeline data"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 62179; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (f.memory_square ? 92159U : 147455U)) { start_rptbd_square(variant + 1); return true; }
			if (f.memory_square) { osd_printf_info("TMS320C54x RPTBD memory SQUR: PASS cases=92160 restore=92160 destinations=2 layouts=first,second,both_same,both_split,SQUR_MPYA forms=DP,indirect,postincrement patterns=8 flags=16 counts=0,1,7 pages=2 snapshots=4 source_reads=once delay_words=once t_publish=Read newer_t_preserved=1 smul_product_clamp=0 body_observes_result=1 reset_before_restore=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated RPTBD memory SQUR complete"); }
			osd_printf_info("TMS320C54x RPTBD square delays: PASS cases=147456 restore=147456 operations=SQURA,SQURS destinations=A,B layouts=first,second,both_same,both_split forms=DP,indirect,postincrement patterns=8 flags=16 counts=0,1,7 pages=2 snapshots=4 source_reads=once delay_words=once t_publish=Read newer_t_preserved=1 body_observes_result=1 count_plus_one=1 reset_before_restore=1 interrupt_composition=unvalidated physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated RPTBD square delays complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62179) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62178; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 62176)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, source = (variant / 16) % 3;
		bool const selected = bool(variant & 1) != bool(variant & 2); unsigned const checkpoint = std::array<unsigned, 4>{3, 5, 8, 12}[variant / 48];
		if (tick == 7 && selected && source) m_cpu->set_input_line(source == 2 ? INPUT_LINE_NMI : 0, ASSERT_LINE);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "XC return protection must not leak into a later ordinary status write");
		if (m_pipeline_operand_reads && !m_pipeline_register_writes) { auto const disabled = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x3000, 0xdead); }
		if (tick == checkpoint && !m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes, m_vector_reads, m_pipeline_extensions_reads}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save XC return, resumed protection and pending vector"); }
		if (tick == 32)
		{
			if (m_vector_reads != unsigned(selected && source)) osd_printf_info("xc_return_vector: case=%u replay=%u vectors=%u ifr=%llx imr=%llx st1=%llx\n", variant, unsigned(m_pipeline_replay), m_vector_reads, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_IFR), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_IMR), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_ST1));
			bool const valid = m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == unsigned(selected) && m_pipeline_store_writes == unsigned(selected) && m_pipeline_register_writes == unsigned(selected && source) && m_vector_reads == unsigned(selected && source) && (source || m_pipeline_extensions_reads == unsigned(selected)) && m_cpu->state_int(tms320c54x_device::STATE_SP) == (selected && !source ? 0x3001 : 0x3000) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (selected ? (m_pipeline_extensions_reads ? 0x340 : 0x100) | (source ? 0x800 : 0) : 0x100);
			if (!valid) osd_printf_info("xc_return_final: case=%u replay=%u selected=%u source=%u read=%u write=%u frame=%u vector=%u status_write=%u sp=%llx st1=%llx pc=%llx idle=%llu\n", variant, unsigned(m_pipeline_replay), unsigned(selected), source, m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes, m_vector_reads, m_pipeline_extensions_reads, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_SP), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_ST1), (unsigned long long)m_cpu->state_int(STATE_GENPC), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_IDLE));
			expect(valid, "Selected and annulled XC returns preserve one target sample, balanced stack effects and bounded protection");
			// Do not queue synchronized input changes before read_stream:
			// it restores scheduler timers, not device_input's pending queue.
			// IFR/NMI and any accepted entry are already in the CPU snapshot;
			// a fresh assertion would duplicate the original physical event.
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_register_writes = m_recurring_irq_snapshot[2]; m_vector_reads = m_recurring_irq_snapshot[3]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[4]; m_check_timer->adjust(attotime::never); m_cpu->set_pipeline_frontend(false); m_cpu->set_pipeline_frontend(true); expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Restore XC return protection without retaining future pipeline state"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 62177; m_check_timer->enable(true); return true; }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_vector_prefetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 191) { start_xc_return(variant + 1); return true; }
			osd_printf_info("TMS320C54x XC return protection: PASS cases=192 restore=192 predicates=BEQ,BNEQ outcomes=2 resumed_words=one,two pages=2 snapshots=4 sources=none,IRQ,NMI irq_selected_only=1 return_reads=once resumed_write=once later_STM_ST1=ordinary reset_before_restore=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated XC return protection complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62177) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62176; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_delayed_far_call_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 253670)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, layout = variant % 3, caller_page = std::array<unsigned, 3>{0, 2, 0x7f}[(variant / 3) % 3], interrupt = (variant / 27) % 3;
		unsigned const checkpoint = std::array<unsigned, 3>{6, 8, 14}[variant / 81];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Delayed far call and both delay layouts remain admitted");
		if (tick == 5 && interrupt) m_cpu->set_input_line(interrupt == 2 ? INPUT_LINE_NMI : 0, ASSERT_LINE);
		capture_pipeline_at(checkpoint, {&m_pipeline_store_writes, &m_fast_stack_reads, &m_pipeline_extensions_reads, &m_pipeline_operand_reads, &m_pipeline_drained_fetches, &m_vector_acks, &m_pipeline_register_writes},
			"Save delayed far call with retained frame and settled input");
		if (tick == 50)
		{
			// Mapped XPC visibility during delay Read is not fixed by the instruction manual.
			if ((layout != 0 && m_cpu->state_int(tms320c54x_device::STATE_A) != (layout == 2 ? 0x4567 : 0)) || m_cpu->state_int(tms320c54x_device::STATE_B) != 0x55 || m_cpu->state_int(tms320c54x_device::STATE_T) != (layout == 1 ? 0x1234 : 0xbeef) || m_cpu->state_int(tms320c54x_device::STATE_BRC) != (layout == 0 ? 0x1234 : 0x9876) || m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1000 || m_cpu->state_int(tms320c54x_device::STATE_XPC) != caller_page)
				osd_printf_info("FCALLD failure: case=%u replay=%u layout=%u source_page=%u irq=%u A=%llx B=%llx T=%llx BRC=%llx SP=%llx XPC=%llx PC=%llx writes=%u reads=%u target=%u continuation=%u acks=%u\n", variant, unsigned(m_pipeline_replay), layout, caller_page, interrupt, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_T), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_BRC), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_SP), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_XPC), (unsigned long long)m_cpu->state_int(STATE_GENPC), m_pipeline_store_writes, m_fast_stack_reads, m_pipeline_register_writes, m_pipeline_drained_fetches, m_vector_acks);
			expect((layout == 0 || m_cpu->state_int(tms320c54x_device::STATE_A) == (layout == 2 ? 0x4567 : 0)) && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x55 && m_cpu->state_int(tms320c54x_device::STATE_T) == (layout == 1 ? 0x1234 : 0xbeef) && m_cpu->state_int(tms320c54x_device::STATE_BRC) == (layout == 0 ? 0x1234 : 0x9876) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && m_cpu->state_int(tms320c54x_device::STATE_XPC) == caller_page, "FCALLD executes caller-fetched delay words and returns to its stacked page and continuation");
			expect(m_pipeline_store_writes == 2 + unsigned(bool(interrupt)) && m_fast_stack_reads == m_pipeline_store_writes && m_pipeline_extensions_reads == 1 && m_pipeline_operand_reads == unsigned(layout == 1) && m_pipeline_drained_fetches == 1 && m_pipeline_register_writes == 1 && m_vector_acks == unsigned(bool(interrupt)), "Far call, delay and IRQ frames each own exactly their accesses");
			if (replay_pipeline_at(253680, checkpoint,
				"Replay caller page, stacked continuation and complete delayed call")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_vector_prefetch_tap.remove(); m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); m_cpu->set_pipeline_frontend(false);
			if (variant < 242) { start_delayed_far_call(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged FCALLD: PASS cases=243 restore=243 caller_pages=3 target_pages=3 layouts=probe_XPC_STLM,STM,original_NOP_LDM sources=none,IRQ,NMI frame_words=2 complete_delays=2 return_roundtrip=1 xpc_delay_visibility=unvalidated silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated delayed far call complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253680) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253670; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_block_branch_count_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 254360)
	{
		// Input changes are scheduler events, not yet saved CPU latches.
		// Defer a coincident snapshot until the next emulated clock.
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, count = block_branch_count(variant), interrupt = variant / 4096, arrival = std::array<unsigned, 4>{9, 10, 12, 14}[(variant / 1024) % 4], requested_checkpoint = std::array<unsigned, 8>{3, 8, 9, 10, 11, 12, 15, 24}[(variant % 1024) / 128], checkpoint = requested_checkpoint + unsigned(interrupt && requested_checkpoint == arrival);
		bool const endpoint = (variant / 16) % 2, b = (variant / 32) % 2;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Forward block branches must use owned branch and loop state");
		if (interrupt && tick == arrival) m_cpu->set_input_line(interrupt == 2 ? INPUT_LINE_NMI : 2, ASSERT_LINE);
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_pipeline_register_writes, &m_fast_stack_reads, &m_vector_acks},
			"Save in-block branch decisions, held words and speculative loop state");
		if (tick == 256)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == count + 1 && m_pipeline_store_writes == count + 1 && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x5001 + count && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x5101 + count && !m_cpu->state_int(tms320c54x_device::STATE_BRC) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (interrupt ? 0x100 : 0x900) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 && m_pipeline_register_writes == unsigned(bool(interrupt)) && m_fast_stack_reads == unsigned(bool(interrupt)) && m_vector_acks == unsigned(bool(interrupt)), "In-block branches complete the loop once with exact counters, pointers, interrupt frame and status");
			expect(m_cpu->state_int(b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == block_branch_source(variant, count) + unsigned(!endpoint) && m_cpu->state_int(b ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B) == (b ? 0x1234 : 0x2345), "Both branch arms join the endpoint without corrupting the other accumulator");
			if (replay_pipeline_at(254370, checkpoint,
				"Replay block branches without duplicate body reads or writes")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 12287) { start_block_branch(variant + 1); return true; }
			osd_printf_info("TMS320C54x in-block branches: PASS cases=12288 restore=12288 layouts=diamond,endpoint counts=0,1,2,7 patterns=4 accumulators=2 pages=2 snapshots=8 arrivals=9,10,12,14 sources=none,IRQ,NMI source_reads_each=1 selected_writes_each=1 brc_decode=1 braf_final_decode=1 held_tags_restored=1 interrupt_return=1 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated in-block branches complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254370) { m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); m_phase = 254371; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 254371) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254360; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_block_xc_edge_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 255700)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, checkpoint = std::array<unsigned, 3>{3, 7, 9}[variant / 6];
		if (tick == checkpoint && !m_pipeline_replay) { expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save before and after cross-end XC rejection"); }
		if (tick == 20)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 3 && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x5100 && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x5200, "Cross-end XC stays excluded before counter or protected-slot effects");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; return restore_pipeline_at(255710, "Restore cross-end exclusion without issuing protected words"); }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 17) { start_block_xc_edge(variant + 1); return true; }
			osd_printf_info("TMS320C54x block XC exclusions: PASS cases=18 restore=18 forms=XC1,XC2 selected_span_crossings=3 pages=2 snapshots=3 data_reads=0 data_writes=0 pointer_effects=0 silicon_trap_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated block XC exclusions complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255710) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255700; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 255680)
	{
		auto const f = block_xc_case(m_pipeline_peripheral_case); unsigned const tick = ++m_idle_fixture_tick, checkpoint = std::array<unsigned, 4>{4, 11, 13, 24}[f.snapshot];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Conditional block slots remain staged");
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_pipeline_register_writes},
			"Save block counters and selected or rejected XC slots");
		if (tick == 120 + f.count * 12)
		{
			u64 const result = u64(s64(s16(f.word(f.count)))) & 0xffffffffffULL;
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == f.count + 1 && m_pipeline_store_writes == f.first() && m_pipeline_register_writes == f.second() && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x5001 + f.count && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x5100 + f.first() && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x5200 + f.second() && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x900 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0xa608 | (f.rotation ? 0x1800 : 0)) && m_cpu->state_int(tms320c54x_device::STATE_A) == (f.b() ? 0x123456789aULL : result) && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.b() ? result : 0xfedcba9876ULL), "XC preserves per-iteration selection, counters, status and exact pointer effects");
			if (replay_pipeline_at(255690, checkpoint,
				"Restore XC decisions without duplicate source or store transactions")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case < 8703) { start_block_xc(m_pipeline_peripheral_case + 1); return true; }
			osd_printf_info("TMS320C54x block XC: PASS cases=8704 restore=8704 predicates=17 layouts=XC1_single,XC2_two_single,XC2_one_double,XC1_penultimate counts=0,1,2,7 positions=interior,endpoint pages=2 snapshots=4 source_rotations=2 ordered_reads=1 selected_writes=1 rejected_writes=0 opcode_literal=1 counter_braf_owned=1 physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated block XC complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255690) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255680; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_move_repeat_owner_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 299000)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, checkpoint = variant & 32 ? 7 : 4;
		if (tick == checkpoint && !m_pipeline_replay) { expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save mapped move alongside independent repeat header"); }
		if (tick == 40)
		{
			bool const rejected = variant & 1;
			expect(bool(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)) == rejected && m_cpu->state_int(tms320c54x_device::STATE_AR2) == (rejected ? 0x3333 : variant & 4 ? 0x5aa5 : 0x1400), "Repeat guard belongs to its body, not an older mapped move");
			if (!rejected) expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE), "Independent repeat and its continuation finish");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_cpu->set_pipeline_frontend(true); return restore_pipeline_at(299010, "Restore repeat ownership after clearing live pipeline"); }
			m_cpu->set_pipeline_frontend(false);
			if (variant < 63) { start_move_repeat_owner(variant + 1); return true; }
			osd_printf_info("TMS320C54x mapped move repeat ownership: PASS cases=64 restore=64 before_header=1 repeated_body_rejected=1 headers=short,long pages=2 values=2 counts=0,3 snapshots=2 physical_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated repeat ownership complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 299010) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 299000; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_block_branch_edge_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 254380)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, checkpoint = variant < 16 ? 3 : 8;
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes},
			"Save before excluded in-block control decoding");
		if (tick == 16)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && !m_pipeline_store_writes, "Unaccepted in-block control remains fail-closed without a control frame or body write");
			if (replay_pipeline_at(254390, checkpoint,
				"Restore excluded branch without atomic fallback")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 31) { start_block_branch_edge(variant + 1); return true; }
			osd_printf_info("TMS320C54x in-block branch exclusions: PASS cases=32 restore=32 kinds=8 pages=2 snapshots=2 below_block_targets=1 endpoint_overlap=1 delayed_control=1 conditional_call=1 nested_blocks=1 far_branch=1 data_writes=0 atomic_fallback=0 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated in-block branch exclusions complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254390) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254380; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_delay_dld_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
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
	if (m_phase == 255240)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = block_back_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Internal backward branch retains the active outer block");
		capture_pipeline_at(f.checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_pipeline_register_writes, &m_fast_stack_reads, &m_vector_acks},
			"Save backward block branch predicate held word and interrupt state");
		if (tick == 2000)
		{
			unsigned const total = (f.outer + 1) * f.inner;
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + f.end + 2 && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_RSA) == 0x602 && m_cpu->state_int(tms320c54x_device::STATE_REA) == f.end && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa002 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000, "Backward inner loop completes the outer count and final BRAF once");
			expect(m_pipeline_operand_reads == total && m_pipeline_store_writes == total && m_pipeline_register_writes == unsigned(bool(f.interrupt)) && m_fast_stack_reads == unsigned(bool(f.interrupt)) && m_vector_acks == unsigned(bool(f.interrupt)) && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x5000 + total && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x6000 + total && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x7001 + f.outer && m_cpu->state_int(tms320c54x_device::STATE_A) == (f.b ? 0x123456789aULL : 0xffffffffffULL) && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.b ? 0xffffffffffULL : 0xfeabcd9876ULL) && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5555, "Backward branches preserve ordered memory work and independent inner/outer counters");
			if (replay_pipeline_at(255250, f.checkpoint,
				"Restore backward inner loop without duplicate copies or lost outer ownership")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 1535) { start_block_back(variant + 1); return true; }
			osd_printf_info("TMS320C54x backward in-block branches: PASS cases=1536 restore=1536 outer_counts=4 inner_counts=4 forms=conditional_back,unconditional_back accumulators=2 pages=2 sources=none,IRQ,NMI snapshots=4 ordered_copies=1 outer_counts_preserved=1 held_words_restored=1 physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated backward in-block branches complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255250) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255240; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 255200)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		auto const f = block_far_call_case(variant, profile_is("rptb-fcalld"));
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Far call preserves its enclosing block while executing outside it");
		capture_pipeline_at(f.checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_pipeline_register_writes, &m_fast_stack_reads, &m_vector_acks, &m_pipeline_extensions_reads},
			"Save active block across far call stack callee or interrupt state");
		if (tick == 1000)
		{
			unsigned const iterations = f.count + 1, stack = iterations * 2 * f.depth + unsigned(bool(f.interrupt));
			if (m_pipeline_operand_reads != iterations || m_pipeline_store_writes != iterations || m_pipeline_register_writes != stack || m_fast_stack_reads != stack || m_vector_acks != unsigned(bool(f.interrupt)) || m_cpu->state_int(tms320c54x_device::STATE_AR6) != 0x7206 + (f.depth == 2 ? iterations : 0))
				osd_printf_info("block_far_call_effects: case=%u replay=%u read=%u write=%u stack_write=%u stack_read=%u expected_stack=%u ack=%u ar2=%llx ar3=%llx ar4=%llx ar5=%llx ar6=%llx t=%llx\n", variant, unsigned(m_pipeline_replay), m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes, m_fast_stack_reads, stack, m_vector_acks, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_AR2), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_AR3), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_AR4), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_AR5), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_AR6), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_T));
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.caller + 0x60b && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_RSA) == 0x602 && m_cpu->state_int(tms320c54x_device::STATE_REA) == 0x609 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608, "Far call returns to the caller page and completes the original block without count or BRAF corruption");
			expect(m_pipeline_operand_reads == iterations && m_pipeline_store_writes == iterations && m_pipeline_register_writes == stack && m_fast_stack_reads == stack && m_vector_acks == unsigned(bool(f.interrupt)) && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x5000 + iterations && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x6000 + iterations && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x7000 + iterations && m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x7100 + (!f.delayed || !f.delay_form ? iterations : 0) && m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x7206 + (f.depth == 2 ? iterations : 0) && m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0x7207 + (f.delayed && !f.delay_form ? iterations : 0) && m_cpu->state_int(tms320c54x_device::STATE_A) == (f.delayed && f.delay_form == 2 ? 1 : 0x123456789aULL) && m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfeabcd9876ULL && m_cpu->state_int(tms320c54x_device::STATE_T) == (f.delayed && f.delay_form == 1 ? 0x1234 : 0x5555) && m_pipeline_extensions_reads == (f.delayed && f.delay_form == 1 ? iterations : 0), "Nested far calls, delay words and optional interrupt balance every stack and data transaction once");
			if (replay_pipeline_at(255210, f.checkpoint,
				"Restore far callee and caller loop with pending interrupt and retained stack")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (f.delayed ? 2303 : 767)) { start_block_far_call(variant + 1); return true; }
			if (f.delayed) { osd_printf_info("TMS320C54x in-block delayed far calls: PASS cases=2304 restore=2304 counts=0,1,2,7 positions=middle,endpoint delay_layouts=two_single,one_double,original_LD_NOP caller_pages=2 callee_pages=same,cross depths=1,2 sources=none,IRQ,NMI snapshots=4 ordered_copy=1 balanced_stack=1 return_pc=call+4 caller_loop_preserved=1 delay_effects=once physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated in-block delayed far calls complete"); }
			osd_printf_info("TMS320C54x in-block far calls: PASS cases=768 restore=768 counts=0,1,2,7 layouts=early,near_end caller_pages=2 callee_pages=same,cross depths=1,2 sources=none,IRQ,NMI snapshots=4 ordered_copy=1 balanced_stack=1 caller_loop_preserved=1 physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated in-block far calls complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255210) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255200; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 255160)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = block_single_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Single repeat inside a block uses independent RPTC and BRC ownership");
		capture_pipeline_at(f.checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_pipeline_extensions_reads},
			"Save inner repeat with active outer block ownership");
		if (tick == 600)
		{
			if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE) || m_cpu->state_int(STATE_GENPC) != f.base + f.end + 4 || m_cpu->state_int(tms320c54x_device::STATE_BRC) || m_cpu->state_int(tms320c54x_device::STATE_ST1) != (f.memory && f.count_form == 1 ? 0x4800 : 0x800) || m_cpu->state_int(tms320c54x_device::STATE_T) != 0x1a2b)
				osd_printf_info("block_single_state: case=%u pc=%llx end=%04x idle=%llu brc=%llu rc=%llu st1=%llx t=%llx reads=%u writes=%u\n", variant, (unsigned long long)m_cpu->state_int(STATE_GENPC), f.end, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_IDLE), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_BRC), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_RC), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_ST1), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_T), m_pipeline_operand_reads, m_pipeline_store_writes);
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + f.end + 4 && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_RC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_RSA) == 0x602 && m_cpu->state_int(tms320c54x_device::STATE_REA) == f.end && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (f.memory && f.count_form == 1 ? 0x4800 : 0x800) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x8820 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x1a2b, "Inner repeat preserves outer completion count BRAF and fallthrough");
			if (f.memory) expect(m_pipeline_extensions_reads == f.outer + 1 && m_cpu->state_int(tms320c54x_device::STATE_AR0) == 0x1060 + (f.count_form == 3 ? f.outer + 1 : 0), "Memory repeat retains one count per outer iteration and updates its pointer once");
			expect(m_pipeline_operand_reads == (f.kind == 0 ? f.total : 0) && m_pipeline_store_writes == (f.kind < 2 ? f.total : 0) && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x5000 + (f.kind == 0 ? f.total : 0) && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x6000 + (f.kind < 2 ? f.total : 0) && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x7000 + f.outer + 1 && m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x7100 + (f.tail ? 0 : f.outer + 1) && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfeabcd9876ULL, "Inner repeat composes ordered memory effects with one prefix and suffix per outer iteration");
			if (replay_pipeline_at(255170, f.checkpoint,
				"Restore inner and outer repeat without lost or duplicate bus work")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (f.memory ? 3839 : 767)) { start_block_single(variant + 1); return true; }
			if (f.memory) { osd_printf_info("TMS320C54x memory repeat in block: PASS cases=3840 restore=3840 forms=DP,SP,indirect,postincrement,absolute outer_counts=4 inner_counts=4 bodies=MVDD,ST,NOP positions=middle,end pages=2 snapshots=4 fresh_counts=1 consumed_counts_poisoned=1 outer_counts_preserved=1 physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated memory repeat in block complete"); }
			osd_printf_info("TMS320C54x single repeat in block: PASS cases=768 restore=768 outer_counts=4 inner_counts=4 bodies=MVDD,ST,NOP positions=middle,end pages=2 snapshots=4 ordered_memory=1 outer_counts_preserved=1 duplicate_transactions=0 interrupt_composition=unvalidated physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated single repeat in block complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 255170) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 255160; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_block_near_call_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 62150)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = block_near_call_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Near call preserves its caller block outside the loop address range");
		capture_pipeline_at(f.checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_pipeline_register_writes, &m_fast_stack_reads, &m_vector_acks},
			"Save block near call stack callee and return state");
		if (tick == 1800)
		{
			unsigned const iterations = f.count + 1, stack = iterations * f.depth + unsigned(bool(f.interrupt));
			if (m_pipeline_operand_reads != iterations || m_pipeline_store_writes != iterations || m_pipeline_register_writes != stack || m_fast_stack_reads != stack || m_cpu->state_int(STATE_GENPC) != f.base + f.end + 11)
				osd_printf_info("block_near_call_effects: case=%u replay=%u pc=%llx reads=%u writes=%u stack_writes=%u stack_reads=%u expected_stack=%u brc=%llx st1=%llx ar4=%llx ar5=%llx ar6=%llx ar7=%llx\n", variant, unsigned(m_pipeline_replay), (unsigned long long)m_cpu->state_int(STATE_GENPC), m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes, m_fast_stack_reads, stack, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_BRC), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_ST1), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_AR4), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_AR5), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_AR6), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_AR7));
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + f.end + 11 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 && m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_RSA) == 0x602 && m_cpu->state_int(tms320c54x_device::STATE_REA) == f.end && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_AR1) == 0x6d01, "Near return completes the caller block and its single post-loop sentinel");
			expect(m_pipeline_operand_reads == iterations && m_pipeline_store_writes == iterations && m_pipeline_register_writes == stack && m_fast_stack_reads == stack && m_vector_acks == unsigned(bool(f.interrupt)), "Block near calls balance every data stack and interrupt transaction");
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x5000 + iterations && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x6000 + iterations && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x7000 + (f.layout != 2 ? iterations : 0) && m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x7100 + (f.delayed || f.layout == 0 ? iterations : 0) && m_cpu->state_int(tms320c54x_device::STATE_AR6) == 0x7200 + (f.depth == 2 ? iterations : 0) && m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0x7300 + (f.delayed_return ? f.depth * iterations : 0) && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0xfeabcd9876ULL && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5555, "Near call and return delays run once per caller iteration without register corruption");
			if (replay_pipeline_at(62151, f.checkpoint,
				"Restore block near call without duplicated memory or frames")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 3455) { start_block_near_call(variant + 1); return true; }
			osd_printf_info("TMS320C54x in-block near calls: PASS cases=3456 restore=3456 counts=0,1,5,15 layouts=middle,endpoint,whole_block pages=3 depths=1,2 sources=none,IRQ,NMI calls=CALL,CALLD returns=RET,RETD snapshots=4 ordered_copy=1 balanced_stack=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated in-block near calls complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62151) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62150; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_block_exit_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase == 62064)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		auto const f = block_exit_case(variant, profile_is("rptb-exit-delay"));
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Forward block exits and spaced outside BRAF clear are admitted");
		capture_pipeline_at(f.checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_pipeline_register_writes, &m_pipeline_extensions_reads, &m_fast_stack_reads},
			"Save block exit with held words, pending endpoint predictions and software-clear state");
		if (tick == 256)
		{
			if (f.delayed) expect(m_fast_stack_reads == f.reads && m_pipeline_extensions_reads == f.reads &&
				m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x5300 + f.reads && m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x5400 + f.reads,
				"Every taken or untaken delayed exit owns exactly two delay-word effects");
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == f.reads && m_pipeline_store_writes == f.writes && m_pipeline_register_writes == unsigned(f.taken) &&
				m_cpu->state_int(tms320c54x_device::STATE_BRC) == (f.taken ? f.count - f.exit : 0) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x900 &&
				m_cpu->state_int(tms320c54x_device::STATE_RSA) == 0x602 && m_cpu->state_int(tms320c54x_device::STATE_REA) == f.end &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x5000 + f.reads && m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x5100 + f.writes &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000, "Exit, explicit BRAF clear and spaced old-endpoint revisit preserve loop registers and exact body effects");
			if (replay_pipeline_at(62065, f.checkpoint,
				"Restore block exit without lost held tags or duplicated outside publication")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (f.delayed ? 9983U : 1151U)) { start_block_exit(variant + 1); return true; }
			if (f.delayed)
			{
				osd_printf_info("TMS320C54x delayed forward block exits: PASS cases=9984 restore=9984 branches=BD,BCD_signed12 counts=0,1,2,7 exits=first,middle,last,never endpoint_layouts=2 delay_layouts=2 pages=2 snapshots=6 delay_words_once=1 decision_before_opposite_load=1 braf_not_auto_cleared=1 outside_rsbx=1 spaced_endpoint_revisit=1 interrupt_composition=unvalidated physical_phase_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated delayed forward block exits complete");
			}
			osd_printf_info("TMS320C54x forward block exits: PASS cases=1152 restore=1152 branches=B,BC_AEQ,BC_BEQ counts=0,1,2,7 exits=first,middle,last,never endpoint_layouts=2 pages=2 snapshots=6 source_reads=ordered body_writes=selected braf_not_auto_cleared=1 outside_rsbx=1 spaced_endpoint_revisit=1 interrupt_composition=unvalidated silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated forward block exits complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62065) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62064; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

} // namespace c54x_test
