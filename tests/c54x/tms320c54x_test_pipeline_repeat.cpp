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
