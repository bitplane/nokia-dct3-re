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


bool tms320c54x_test_state::check_branch_repeat_results()
{
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



bool tms320c54x_test_state::check_delayed_call_replay_results()
{
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

bool tms320c54x_test_state::check_block_xc_results()
{
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

} // namespace c54x_test
