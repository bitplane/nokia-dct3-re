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



tms320c54x_test_state::delay_asm_fixture tms320c54x_test_state::delay_asm_case(unsigned v)
{
	delay_asm_fixture f{}; f.control = v % 4; v /= 4; f.form = v % 5; v /= 5;
	f.raw = v % 32; v /= 32; f.pattern = v % 2; v /= 2; f.flags = v % 4; v /= 4;
	f.base = v % 2 ? 0x20000 : 0; v /= 2;
	int const shift = f.raw < 16 ? int(f.raw) : int(f.raw) - 32;
	u64 const source = f.pattern ? 0xfffffffff9ULL : 7;
	if (shift >= 0) f.result = (source << shift) & 0xffffffffffULL;
	else if (f.pattern && (f.flags & 1)) f.result = u64(-((7 + (s64(1) << -shift) - 1) >> -shift)) & 0xffffffffffULL;
	else f.result = source >> -shift;
	f.checkpoint = std::array<unsigned, 3>{3 + f.prefix(), f.read(), f.read() + 1}[v];
	return f;
}

void tms320c54x_test_state::start_delay_asm(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = delay_asm_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 24; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	if (f.prefix()) program.write_word(f.base + 0x600, f.form == 1 ? 0x8000 : 0x8002);
	unsigned const start = 0x600 + f.prefix();
	program.write_word(f.base + start, f.control == 1 || f.control == 2 ? 0xfa45 : 0xf273); program.write_word(f.base + start + 1, 0x700);
	program.write_word(f.base + start + 2, std::array<u16, 5>{0x3202, 0x3200, 0x3293, 0x32e3, 0x32f8}[f.form]);
	program.write_word(f.base + start + 3, f.long_form() ? f.form == 3 ? 3 : 0x1002 : 0xf682);
	if (f.long_form()) program.write_word(f.base + f.target(), 0xf682);
	program.write_word(f.base + f.store(), 0x880f); program.write_word(f.base + f.store() + 1, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa620); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1() | (f.raw ^ 31));
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.prefix() ? 0x1234000000ULL | f.source_word() : f.control == 1 ? 0 : f.control == 2 ? 1 : 0x123456789aULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, f.pattern ? 0xfffffffff9ULL : 7); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x5aa5); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1002);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, i == 3 ? f.form == 3 ? 0xfff : 0x1002 : 0x5100 + i);
	data.write_word(0x1002, f.prefix() ? 0xdead : f.source_word());
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "delay_asm_source", [this, f](offs_t address, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(address == 0x1002 && word == f.source_word() && !m_pipeline_operand_reads++ && m_cpu->pipeline_cycles() == f.read(), "Delay ASM captures its DP/SP/indirect operand once at Read"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "delay_asm_writes", [this, f](offs_t address, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (address == 0x1002) { expect(f.prefix() && word == f.source_word() && !m_pipeline_register_writes++ && m_cpu->pipeline_cycles() == 6, "The original-like older store supplies the stack/RAM ASM word"); return; } expect(address == 0xf && word == u16(f.result) && !m_pipeline_store_writes++ && m_pipeline_operand_reads == 1 && m_cpu->pipeline_cycles() >= f.read() + 3, "The selected branch target observes the ASM-shifted result once"); });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + start + 3, f.base + start + 3, "delay_asm_extension", [this, f](offs_t, u16 &word, u16) { if (machine().side_effects_disabled() || !f.long_form()) return; expect(word == (f.form == 3 ? 3 : 0x1002) && !m_pipeline_extensions_reads++ && m_cpu->pipeline_cycles() == f.literal_clock(), "Delay ASM retains its one address extension"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 311000;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_far_return(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	unsigned const form = variant % 36, prefix = variant >= 36;
	u32 const base = form & 1 ? 0x10000 : 0;
	u32 const target = (base ^ 0x10000) + 0x700;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 20; ++i) { program.write_word(base + 0x600 + i, 0xe933); program.write_word((target & 0x10000) + 0x700 + i, 0xf495); }
	bool const delayed = form >= 12;
	if (prefix) program.write_word(base + 0x600, 0x8a11); // POPM AR1 immediately before the return.
	program.write_word(base + 0x600 + prefix, (form % 12 < 6 ? 0xf4e5 : 0xf4e4) | (delayed ? 0x200 : 0));
	if (delayed) {
		program.write_word(base + 0x601 + prefix, form >= 24 ? 0xf020 : 0xe811);
		program.write_word(base + 0x602 + prefix, form >= 24 ? 0x1234 : 0xe922);
	}
	program.write_word(target, 0xe955);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST1, 0x800}, {cpu_regs::STATE_SP, 0x1000 - prefix}, {cpu_regs::STATE_AR0 + 1, 0},
		{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_IFR, 0},
	});
	m_cpu->space(AS_DATA).write_word(0x1000, 0x80 | (target >> 16));
	m_cpu->space(AS_DATA).write_word(0x1001, 0x700);
	m_cpu->space(AS_DATA).write_word(0x0fff, 0x42);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0x1000, 0x1001, "staged_far_return_stack",
		[this, target, delayed, prefix](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(address == 0x1000 + m_pipeline_operand_reads && value == (address == 0x1000 ? (0x80 | (target >> 16)) : 0x700) &&
				m_cpu->pipeline_cycles() == (delayed ? 4 : 5) + prefix + m_pipeline_operand_reads, "Far return samples XPC then PC once in the declared retained-return model");
			++m_pipeline_operand_reads;
		});
	m_pipeline_fetch_tap = program.install_read_tap(target, target, "staged_far_return_target",
		[this, delayed, prefix](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == (delayed ? 6 : 8) + prefix, "Far-return target Fetch follows plain or delayed control redirection"); ++m_pipeline_extensions_reads;
		});
	m_phase = 251290; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_conditional_branch(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	bool const b = BIT(variant, 0), eq = BIT(variant, 1), nonzero = BIT(variant, 2);
	u32 const base = BIT(variant, 3) ? 0x10000 : 0;
	bool const taken = eq ? !nonzero : nonzero;
	bool const memory_dp = variant >= 48;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) { program.write_word(base + 0x600 + i, 0xf495); program.write_word(base + 0x700 + i, 0xf495); }
	write_words(program, {
		{base + 0x600, 0xfa44 | (b << 3) | eq}, {base + 0x601, 0x700}, {base + 0x602, memory_dp ? 0x467f : 0xe811},
		{base + 0x603, memory_dp ? 0xf495 : 0xe944}, {base + 0x604, 0xe966},
		{base + 0x700, memory_dp ? 0x107e : 0xe955},
	});
	if (memory_dp) program.write_word(base + 0x701, 0xe955);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, 0xa608}, {cpu_regs::STATE_ST1, 0x800},
		{cpu_regs::STATE_A, b ? 0x9999 : nonzero ? 0x0100000000ULL : 0},
		{cpu_regs::STATE_B, b ? (nonzero ? 0x0100000000ULL : 0) : 0x7777}, {cpu_regs::STATE_IFR, 0},
		{cpu_regs::STATE_IMR, 0},
	});
	if (memory_dp) {
		m_cpu->space(AS_DATA).write_word(0x47f, 0xe012);
		m_cpu->space(AS_DATA).write_word(0x97e, 0x3333);
	}
	m_cpu->set_state_int(STATE_GENPC, base + 0x600);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_drained_fetches = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = program.install_read_tap(base + 0x700, base + 0x700, "staged_conditional_target",
		[this, taken, memory_dp](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(taken && m_cpu->pipeline_cycles() == (memory_dp ? 10 : 8), "BCD uses the retained predicate and conditional prefetch bubble");
			++m_pipeline_operand_reads;
		});
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x602, base + 0x603, "staged_conditional_delays",
		[this, base, memory_dp](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_pipeline_extensions_reads < 2 && address == base + 0x602 + m_pipeline_extensions_reads &&
				value == (memory_dp ? (m_pipeline_extensions_reads ? 0xf495 : 0x467f) : (m_pipeline_extensions_reads ? 0xe944 : 0xe811)) &&
				m_cpu->pipeline_cycles() == (m_pipeline_extensions_reads ? 6 : 4), "BCD fetches both delay words with one intervening bubble");
			++m_pipeline_extensions_reads;
		});
	if (memory_dp) m_pipeline_write_tap = m_cpu->space(AS_DATA).install_read_tap(0x47f, 0x97e, "staged_conditional_dp_data",
		[this, taken](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			if (address == 0x47f) {
				expect(value == 0xe012 && m_cpu->pipeline_cycles() == 7, "BCD memory-DP delay samples its original page once");
				++m_pipeline_drained_fetches;
			} else if (address == 0x97e) {
				expect(taken && value == 0x3333 && m_cpu->pipeline_cycles() == 13, "Taken BCD continuation uses DP published by its delay slot");
				++m_pipeline_store_writes;
			}
		});
	m_phase = 251610; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_plain_return(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	bool const pushed = variant >= 12, adjacent = pushed && variant < 24, enable = BIT(variant, 1);
	unsigned const prefix = pushed ? (adjacent ? 1 : 2) : 0;
	u16 const target = adjacent ? 0x720 : 0x700;
	u32 const base = BIT(variant, 0) ? 0x10000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) { program.write_word(base + 0x600 + i, 0xe933); program.write_word(base + 0x700 + i, 0xf495); program.write_word(base + 0x720 + i, 0xf495); }
	if (pushed) program.write_word(base + 0x600, 0x4a11);
	if (prefix == 2) program.write_word(base + 0x601, 0xf495);
	program.write_word(base + 0x600 + prefix, enable ? 0xf4eb : 0xfc00);
	program.write_word(base + 0x700, 0xe955);
	program.write_word(base + 0x720, 0xe999);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, 0xa61b}, {cpu_regs::STATE_ST1, 0x800}, {cpu_regs::STATE_SP, 0x1000},
		{cpu_regs::STATE_AR1, 0x700}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
	});
	m_cpu->space(AS_DATA).write_word(0xfff, 0x720);
	m_cpu->space(AS_DATA).write_word(0x1000, 0x700);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_operand_reads = m_pipeline_extensions_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(pushed ? 0xfff : 0x1000, pushed ? 0xfff : 0x1000, "staged_plain_return_stack",
		[this, prefix, target](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(value == target && m_cpu->pipeline_cycles() == 5 + prefix, "RET samples old DARAM before concurrent push Write; one NOP exposes the new value");
			++m_pipeline_operand_reads;
		});
	m_pipeline_fetch_tap = program.install_read_tap(base + target, base + target, "staged_plain_return_target",
		[this, prefix](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 7 + prefix, "RET target Fetch uses its retained stack sample");
			++m_pipeline_extensions_reads;
		});
	if (pushed) m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0xfff, 0xfff, "staged_plain_return_push",
		[this, adjacent](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(value == 0x700 && m_cpu->pipeline_cycles() == 6 && m_pipeline_operand_reads == unsigned(adjacent), "Push writes in the second half-cycle, after any same-clock return Read");
			++m_pipeline_store_writes;
		});
	m_phase = 251630; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_call(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	bool const t_delay = variant >= 24;
	u16 const parameter = t_delay ? std::array<u16, 9>{6, 0, 0x8000, 0xf495, 0xffff, 0x76e6, 0x4ee6, 0x7001, 0x54e6}[(variant - 24) / 12] : 0;
	unsigned const adjacent = t_delay ? BIT(variant, 1) : variant >= 12;
	bool const delayed = t_delay || BIT(variant, 1);
	u32 const base = BIT(variant, 0) ? 0x10000 : 0;
	u16 const continuation = 0x600 + adjacent + (delayed ? 4 : 2);
	u16 const stack = 0xfff - adjacent;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) { program.write_word(base + 0x600 + i, 0xf495); program.write_word(base + 0x700 + i, 0xf495); }
	if (adjacent) program.write_word(base + 0x600, 0x4a11);
	program.write_word(base + 0x600 + adjacent, delayed ? 0xf274 : 0xf074);
	program.write_word(base + 0x601 + adjacent, 0x700);
	if (delayed) { program.write_word(base + 0x602 + adjacent, t_delay ? 0x770e : 0xe811); program.write_word(base + 0x603 + adjacent, t_delay ? parameter : 0xe922); }
	program.write_word(base + continuation, 0xe966);
	program.write_word(base + 0x700, t_delay ? 0x490e : 0xe955);
	program.write_word(base + 0x701, 0xfc00);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, 0xa61b}, {cpu_regs::STATE_ST1, 0x800}, {cpu_regs::STATE_SP, 0x1000},
		{cpu_regs::STATE_RTN, 0x8888}, {cpu_regs::STATE_T, 0xbeef}, {cpu_regs::STATE_AR1, 0x7777},
		{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
	});
	m_cpu->space(AS_DATA).write_word(0xffe, 0xaaaa);
	m_cpu->space(AS_DATA).write_word(0xfff, 0xaaaa);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_drained_fetches = 0;
	m_pipeline_extensions_reads = 0;
	m_pipeline_register_writes = 0;
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0x0e, 0xfff, "staged_call_stack",
		[this, stack, continuation, adjacent, t_delay, parameter](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			if (address == 0x0e) {
				expect(t_delay && value == parameter && m_cpu->pipeline_cycles() == 7 + adjacent, "CALLD's complete two-word STM delay publishes T once before callee Read");
				++m_pipeline_register_writes;
			} else if (address == stack) {
				expect(value == continuation && m_cpu->pipeline_cycles() == 6 + adjacent, "CALL writes its retained return word in the opcode's Execute clock");
				++m_pipeline_store_writes;
			} else if (address == 0xfff) {
				expect(adjacent && address == 0xfff && value == 0x7777 && m_cpu->pipeline_cycles() == 6, "Older push and younger CALL own distinct stack destinations");
				++m_pipeline_drained_fetches;
			}
		});
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(stack, stack, "staged_call_return_pop",
		[this, continuation, adjacent](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(value == continuation && m_cpu->pipeline_cycles() == 10 + adjacent, "Callee RET reads exactly the caller's retained stack word");
			++m_pipeline_operand_reads;
		});
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x700, base + 0x700, "staged_call_program_observer",
		[this, adjacent](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 6 + adjacent, "CALL target Fetch coincides with its return-address write");
			++m_pipeline_extensions_reads;
		});
	m_phase = 251650; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_worker_xc(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	bool const branch = variant >= 8;
	u32 const origin = branch ? 0x23dbc : 0x600;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) { program.write_word(origin + i, 0xf495); if (branch) program.write_word(0x23ddb + i, 0xf495); }
	std::array<u16, 9> const prefix{0xfd00, 0x4a07, 0xfd00, 0xf7bb, 0x7192, 0x0010, 0x1092, 0xf844, 0x3ddb};
	for (unsigned i = 0; i < (branch ? 9 : variant >= 4 ? 7 : 4); ++i) program.write_word(origin + i, prefix[i]);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, 0xa608}, {cpu_regs::STATE_ST1, 0}, {cpu_regs::STATE_SP, 0x500}, {cpu_regs::STATE_IFR, 0},
		{cpu_regs::STATE_IMR, 0}, {cpu_regs::STATE_AR0, 0x5678}, {cpu_regs::STATE_AR2, 0x510},
		{cpu_regs::STATE_A, 0x1234},
	});
	m_cpu->space(AS_DATA).write_word(0x510, 0xbeef);
	m_cpu->space(AS_DATA).write_word(0x511, branch ? BIT(variant, 0) : 0x1357);
	m_cpu->set_state_int(STATE_GENPC, origin);
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_store_writes = m_pipeline_operand_reads = m_pipeline_extensions_reads = m_pipeline_drained_fetches = 0;
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0x4ff, 0x4ff, "worker_xc_push",
		[this](offs_t, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(word == 0 && m_cpu->pipeline_cycles() == 7, "Original XC prefix saves old ST1 once before setting INTM");
			++m_pipeline_store_writes;
		});
	if (variant >= 4)
		m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0x510, 0x511, "original_worker_reads",
			[this, variant, branch](offs_t address, u16 &word, u16) {
				if (machine().side_effects_disabled()) return;
				expect(word == (address == 0x510 ? 0xbeef : branch ? BIT(variant, 0) : 0x1357) &&
					m_cpu->pipeline_cycles() == (address == 0x510 ? 9 : 11), "Original worker MVDK and adjacent LD sample successive source words once");
				++m_pipeline_operand_reads;
			});
	if (branch)
		m_pipeline_fetch_tap = program.install_read_tap(0x23dc7, 0x23ddb, "original_worker_bc_route",
			[this, variant](offs_t address, u16 &, u16) {
				if (machine().side_effects_disabled()) return;
				if (address != 0x23dc7 && address != 0x23ddb) return;
				expect(bool(BIT(variant, 0)) == (address == 0x23ddb) && m_cpu->pipeline_cycles() == 14, "Original worker BC consumes adjacent load and fetches the selected route once");
				if (address == 0x23ddb) ++m_pipeline_extensions_reads;
				else ++m_pipeline_drained_fetches;
			});
	m_phase = 251690; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
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

void tms320c54x_test_state::start_pipeline_xc_tc(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(0x600 + i, 0xf495);
	bool const producer = variant >= 64;
	unsigned const gap = producer ? (variant - 64) / 16 : 0;
	unsigned const offset = producer ? 1 + gap : 0;
	bool const multiword = variant >= 32 && !producer;
	if (producer) program.write_word(0x600, BIT(variant, 0) ? 0xf4bc : 0xf5bc);
	program.write_word(0x600 + offset, 0xfd20 | ((multiword || BIT(variant, 1)) << 9) | (BIT(variant, 2) << 4));
	program.write_word(0x601 + offset, multiword ? 0x7600 : 0x8000);
	program.write_word(0x602 + offset, multiword ? 0xfd30 : 0x8001);
	if (multiword) program.write_word(0x603, 0x8001);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, 0xa60e | (BIT(variant, 0) << 12)},
	});
	set_registers(*m_cpu, {
		{cpu_regs::STATE_ST1, 0x800}, {cpu_regs::STATE_A, 0x1234}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
		{STATE_GENPC, 0x600},
	});
	m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1; m_pipeline_store_writes = 0;
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0x700, 0x701, "xc_tc_stores",
		[this, variant](offs_t address, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			bool const producer = variant >= 64;
			unsigned const gap = producer ? (variant - 64) / 16 : 0;
			unsigned const offset = producer ? 1 + gap : 0;
			bool const sample = producer && gap < 2 ? BIT(variant, 0) : !BIT(variant, 0);
			bool const execute = sample == BIT(variant, 2);
			bool const multiword = variant >= 32 && !producer;
			expect(word == (multiword && address == 0x700 ? 0xfd30 : 0x1234) &&
				m_cpu->pipeline_cycles() == (multiword ? 8 : 7) + offset + address - 0x700 &&
				(execute || (address == 0x701 && (multiword || !BIT(variant, 1)))), "XC uses Access-sampled TC and rejected words never write data");
			++m_pipeline_store_writes;
		});
	m_phase = 251730; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_plain_branch(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true);
	m_pipeline_peripheral_case = variant;
	bool const b = BIT(variant, 0), eq = BIT(variant, 1), nonzero = BIT(variant, 2);
	bool const taken = eq ? !nonzero : nonzero;
	u32 const base = BIT(variant, 3) ? 0x10000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) { program.write_word(base + 0x600 + i, 0xf495); program.write_word(base + 0x700 + i, 0xf495); }
	program.write_word(base + 0x600, 0xf844 | (b << 3) | eq);
	program.write_word(base + 0x601, 0x700);
	program.write_word(base + 0x602, 0x1070); program.write_word(base + 0x603, 0x1071);
	program.write_word(base + 0x604, 0xe866); program.write_word(base + 0x700, 0xe855);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, 0xa608}, {cpu_regs::STATE_ST1, 0x800},
		{cpu_regs::STATE_A, b ? 0x9999 : nonzero ? 0x0100000000ULL : 0},
		{cpu_regs::STATE_B, b ? (nonzero ? 0x0100000000ULL : 0) : 0x7777}, {cpu_regs::STATE_IFR, 0},
		{cpu_regs::STATE_IMR, 0},
	});
	m_cpu->space(AS_DATA).write_word(0x470, 0x1111); m_cpu->space(AS_DATA).write_word(0x471, 0x2222);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_operand_reads = m_pipeline_extensions_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = program.install_read_tap(base + 0x700, base + 0x700, "plain_bc_target",
		[this, taken](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(taken && m_cpu->pipeline_cycles() == 7, "Taken BC fetches its target after discarding sequential Decode");
			++m_pipeline_operand_reads;
		});
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x602, base + 0x603, "plain_bc_sequential",
		[this, base](offs_t address, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(address == base + 0x602 + m_pipeline_extensions_reads && m_cpu->pipeline_cycles() == (address == base + 0x602 ? 4 : 6), "BC retains each sequential Fetch exactly once across its empty slot");
			++m_pipeline_extensions_reads;
		});
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_read_tap(0x470, 0x471, "plain_bc_data",
		[this, taken](offs_t address, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(!taken && word == (address == 0x470 ? 0x1111 : 0x2222) && m_cpu->pipeline_cycles() == 8 + address - 0x470, "Only not-taken BC decodes sequential data reads");
			++m_pipeline_store_writes;
		});
	m_phase = 251750; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::signed_xc_fixture tms320c54x_test_state::signed_xc_case(unsigned variant)
{
	unsigned const condition = variant % 6, pattern = (variant / 12) % 8;
	bool const b = (variant / 6) % 2;
	u64 const value = std::array<u64, 8>{0, 1, 0x8000000000ULL, 0xffffffffffULL, 0x7fffffffffULL, 0x0100000000ULL, 0x0080000000ULL, 0xff80000000ULL}[pattern];
	s64 const signed_value = value & 0x8000000000ULL ? s64(value) - (s64(1) << 40) : s64(value);
	bool const taken = std::array<bool, 6>{signed_value >= 0, signed_value < 0, value != 0, value == 0, signed_value > 0, signed_value <= 0}[condition];
	u64 const opposite = taken ? std::array<u64, 6>{0x8000000000ULL, 0, 0, 1, 0, 1}[condition] : std::array<u64, 6>{0, 0x8000000000ULL, 1, 0, 1, 0}[condition];
	return {condition, (variant / 96) % 3, b, taken, value, opposite, (variant / 288) % 2 ? 0x20000U : 0};
}

void tms320c54x_test_state::start_signed_xc(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = signed_xc_case(variant); bool const multiword = f.layout == 2;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, 0xfd42 + f.condition + (f.b ? 8 : 0) + (f.layout ? 0x200 : 0));
	program.write_word(f.base + 0x601, multiword ? 0x7600 : 0x8c00);
	program.write_word(f.base + 0x602, multiword ? 0xfd43 : 0x8c01);
	if (multiword) program.write_word(f.base + 0x603, 0x8c01);
	program.write_word(f.base + 0x603 + multiword, 0xf4e1);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_ST0, 0x060e}, {cpu_regs::STATE_ST1, 0x800}, {cpu_regs::STATE_T, 0x5aa5},
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_A, f.b ? 0x1234 : f.opposite},
		{cpu_regs::STATE_B, f.b ? f.opposite : 0x3456}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
	});
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "signed_xc_no_read", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Signed XC slots never read a data operand"); });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "signed_xc_writes",
		[this, f, multiword](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect((at == 0x700 || at == 0x701) && (f.taken || (at == 0x701 && (multiword || !f.layout))) && word == (at == 0x700 && multiword ? 0xfd43 : 0x5aa5) && m_cpu->pipeline_cycles() == 7 + unsigned(multiword) + at - 0x700, "Signed XC retains its Access predicate and never executes a rejected slot"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x602, "signed_xc_words",
		[this, f](offs_t at, u16 &, u16) { if (machine().side_effects_disabled()) return; unsigned const bit = 1U << (at - f.base - 0x601); expect(!(m_pipeline_extensions_reads & bit), "Each protected word is fetched once, including opcode-shaped literal"); m_pipeline_extensions_reads |= bit; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254880;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_delayed_return(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const xc = variant >= 12, two_word = !xc && BIT(variant, 1);
	u32 const base = BIT(variant, 0) ? 0x10000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) { program.write_word(base + 0x600 + i, 0xf495); program.write_word(base + 0x700 + i, 0xf495); program.write_word(base + 0x40 + i, 0xf495); }
	program.write_word(base + 0x600, 0xfe00);
	program.write_word(base + 0x601, xc ? 0xfd45 : two_word ? 0x770e : 0xe811);
	program.write_word(base + 0x602, xc ? 0xf6bb : two_word ? 0x0006 : 0xe944);
	program.write_word(base + 0x603, 0xffff); // Must never become a third delay word.
	program.write_word(base + 0x700, 0xe955);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, 0xa608}, {cpu_regs::STATE_ST1, 0x800}, {cpu_regs::STATE_SP, 0x1000},
	});
	m_cpu->set_state_int(tms320c54x_device::STATE_A, xc && BIT(variant, 1) ? 0x800 : 0);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_B, 0}, {cpu_regs::STATE_T, 0}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, xc ? 1 : 0},
	});
	m_cpu->space(AS_DATA).write_word(0x1000, 0x700);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_operand_reads = m_pipeline_extensions_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0x1000, 0x1000, "retd_stack",
		[this](offs_t, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(word == 0x700 && m_cpu->pipeline_cycles() == 5, "RETD samples its stack return once in Read"); ++m_pipeline_operand_reads;
		});
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x700, base + 0x700, "retd_target",
		[this](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 7, "RETD fetches its retained target after two empty Fetch slots"); ++m_pipeline_extensions_reads;
		});
	m_pipeline_write_tap = program.install_read_tap(base + 0x601, base + 0x603, "retd_delay_fetch",
		[this, base](offs_t address, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(address == base + 0x601 + m_pipeline_store_writes && m_pipeline_store_writes < 2 &&
				m_cpu->pipeline_cycles() == 3 + address - base - 0x601, "RETD fetches two retained delay words once and no third word"); ++m_pipeline_store_writes;
		});
	m_phase = 251790; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}



void tms320c54x_test_state::start_pipeline_retd_data_order(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const prefix = BIT(variant, 1) ? 2 : 1; u16 const target = prefix == 1 ? 0x720 : 0x700;
	u32 const base = BIT(variant, 0) ? 0x10000 : 0; auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) { program.write_word(base + 0x600 + i, 0xf495); program.write_word(base + 0x700 + i, 0xf495); program.write_word(base + 0x720 + i, 0xf495); }
	program.write_word(base + 0x600, 0x4a11); program.write_word(base + 0x600 + prefix, 0xfe00);
	program.write_word(base + 0x603 + prefix, 0xffff);
	program.write_word(base + 0x700, 0xe955); program.write_word(base + 0x720, 0xe999);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST1, 0x800}, {cpu_regs::STATE_SP, 0x1000}, {cpu_regs::STATE_AR1, 0x700},
		{cpu_regs::STATE_B, 0}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
	});
	m_cpu->space(AS_DATA).write_word(0xfff, 0x720);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1; m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0xfff, 0xfff, "retd_read_first",
		[this, prefix, target](offs_t, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(word == target && m_cpu->pipeline_cycles() == 5 + prefix, "RETD reads old DARAM before same-clock push; one NOP exposes completed write"); ++m_pipeline_operand_reads;
		});
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0xfff, 0xfff, "retd_push_write",
		[this, prefix](offs_t, u16 &word, u16) {
			if (machine().side_effects_disabled()) return;
			expect(word == 0x700 && m_cpu->pipeline_cycles() == 6 && m_pipeline_operand_reads == unsigned(prefix == 1), "RETD source Read is ordered before concurrent push Write"); ++m_pipeline_store_writes;
		});
	m_pipeline_fetch_tap = program.install_read_tap(base + target, base + target, "retd_order_target",
		[this, prefix](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 7 + prefix, "RETD retains its sampled old/new target across delay release"); ++m_pipeline_extensions_reads;
		});
	m_phase = 251910; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}


void tms320c54x_test_state::start_pipeline_far_branch(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const target_page = std::array<unsigned, 5>{0, 1, 2, 3, 0x7f}[variant / 8];
	u32 const source = BIT(variant, 0) ? 0x20000 : 0, target = target_page << 16;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) { program.write_word(source + 0x600 + i, 0xf495); program.write_word(target + 0x700 + i, 0xf495); }
	program.write_word(source + 0x600, 0xf880 | target_page); program.write_word(source + 0x601, 0x700);
	program.write_word(source + 0x602, 0xe811); program.write_word(source + 0x603, 0xe922);
	program.write_word(target + 0x700, 0xe844);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, 0xa608}, {cpu_regs::STATE_ST1, 0x800}, {cpu_regs::STATE_SP, 0x1000},
		{cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
	});
	m_cpu->set_state_int(STATE_GENPC, source + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = program.install_read_tap(source + 0x601, source + 0x601, "far_branch_target_literal",
		[this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(value == 0x700 && m_cpu->pipeline_cycles() == 3, "FB retains its target literal from one program Fetch");
			++m_pipeline_operand_reads;
		});
	m_pipeline_fetch_tap = program.install_read_tap(target + 0x700, target + 0x700, "far_branch_target_page",
		[this](offs_t, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(value == 0xe844 && m_cpu->pipeline_cycles() == 6, "FB redirects to its retained seven-bit page without a stack write");
			++m_pipeline_extensions_reads;
		});
	m_phase = 252090; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_pipeline_far_call(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const page = std::array<unsigned, 5>{0, 1, 2, 3, 0x7f}[variant / 16];
	u32 const source = BIT(variant, 0) ? 0x20000 : 0, target = page << 16;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) { program.write_word(source + 0x600 + i, 0xf495); program.write_word(target + 0x700 + i, 0xf495); }
	program.write_word(source + 0x600, 0xf980 | page); program.write_word(source + 0x601, 0x700);
	program.write_word(source + 0x602, 0xe844); program.write_word(source + 0x603, 0xe811);
	program.write_word(target + 0x700, 0xe955); program.write_word(target + 0x701, 0xf4e4); program.write_word(target + 0x702, 0xffff);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, 0xa608}, {cpu_regs::STATE_ST1, 0x800}, {cpu_regs::STATE_SP, 0x1000},
		{cpu_regs::STATE_RTN, 0xbeef}, {cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0}, {cpu_regs::STATE_IFR, 0},
		{cpu_regs::STATE_IMR, 0},
	});
	m_cpu->set_state_int(STATE_GENPC, source + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_pipeline_expected_cycle = 1;
	m_pipeline_store_writes = m_pipeline_operand_reads = m_pipeline_extensions_reads = m_pipeline_drained_fetches = m_fast_stack_reads = 0;
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0xffe, 0xfff, "far_call_stack_pair",
		[this, source](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_pipeline_store_writes < 2 && address == 0xfff - m_pipeline_store_writes &&
				value == (m_pipeline_store_writes ? source >> 16 : 0x602) && m_cpu->pipeline_cycles() == 6 + m_pipeline_store_writes,
				"FCALL writes PC then retained caller XPC on separate clocks");
			++m_pipeline_store_writes;
		});
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0xffe, 0xfff, "far_call_return_pair",
		[this, source](offs_t address, u16 &value, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_fast_stack_reads < 2 && address == 0xffe + m_fast_stack_reads &&
				value == (m_fast_stack_reads ? 0x602 : source >> 16) && m_cpu->pipeline_cycles() == 10 + m_fast_stack_reads,
				"FRET reads its saved caller page then PC without re-reading either operand");
			++m_fast_stack_reads;
		});
	m_pipeline_fetch_tap = program.install_read_tap(target + 0x700, target + 0x700, "far_call_target",
		[this](offs_t, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			expect(m_cpu->pipeline_cycles() == 6, "FCALL target Fetch has class-29B branch timing"); ++m_pipeline_extensions_reads;
		});
	m_vector_prefetch_tap = program.install_read_tap(source + 0x601, source + 0x602, "far_call_literal_and_continuation",
		[this, source](offs_t address, u16 &, u16) {
			if (machine().side_effects_disabled()) return;
			if (address == source + 0x601) { expect(m_cpu->pipeline_cycles() == 3, "FCALL target literal is fetched once"); ++m_pipeline_operand_reads; }
			else { expect(m_cpu->pipeline_cycles() == (m_pipeline_drained_fetches ? 13 : 4), "Caller continuation is first flushed, then refetched after FRET"); ++m_pipeline_drained_fetches; }
		});
	m_phase = 252170; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_delayed_condition_return(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant; auto const f = delayed_condition_return_case(variant);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) { program.write_word(f.base + 0x600 + i, 0xf495); program.write_word(f.base + 0x700 + i, 0xf495); }
	program.write_word(f.base + 0x600, f.opcode);
	program.write_word(f.base + 0x601, f.layout ? 0x7712 : f.condition >= 12 ? (f.st0 & 0x1000 ? 0xf4bc : 0xf5bc) : 0xe800);
	program.write_word(f.base + 0x602, f.layout ? 0xf4e1 : f.condition >= 12 ? 0xe800 : 0xe900);
	for (u32 start : {f.base + 0x603, f.base + 0x700})
	{
		program.write_word(start, 0x80f8); program.write_word(start + 1, 0x1004); program.write_word(start + 2, 0x81f8); program.write_word(start + 3, 0x1005);
		program.write_word(start + 4, start == f.base + 0x700 ? 0xe955 : 0xe933); program.write_word(start + 5, 0xf4e1);
	}
	data.write_word(f.sp, 0x700); data.write_word(0x1004, 0xdead); data.write_word(0x1005, 0xdead);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0}, {cpu_regs::STATE_ST0, f.st0},
		{cpu_regs::STATE_ST1, 0x900}, {cpu_regs::STATE_A, f.a}, {cpu_regs::STATE_B, f.b}, {cpu_regs::STATE_T, 0x5aa5},
		{cpu_regs::STATE_SP, f.sp}, {cpu_regs::STATE_AR2, 0x1234},
	});
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove();
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = m_pipeline_drained_fetches = 0;
	m_pipeline_operand_tap = data.install_read_tap(f.sp, f.sp, "rcd_stack", [this, f](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(f.taken && !m_pipeline_operand_reads && word == 0x700, "Only a taken RCD samples its stack target once"); ++m_pipeline_operand_reads; auto const disabled = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.sp, 0x710); });
	m_pipeline_write_tap = data.install_write_tap(0x1004, 0x1005, "rcd_delay_observation", [this, f](offs_t address, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(address == 0x1004 + m_pipeline_store_writes && word == u16(m_pipeline_store_writes ? f.final_b : f.final_a), "RCD executes its delay instructions once before the selected continuation"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x600, f.base + 0x700, "rcd_fetch", [this, f](offs_t address, u16 &, u16) { if (machine().side_effects_disabled()) return; if (address == f.base + 0x601 || address == f.base + 0x602) { unsigned const bit = 1U << (address - f.base - 0x601); expect(!(m_pipeline_extensions_reads & bit), "RCD retains each delay word without refetch including opcode-shaped literals"); m_pipeline_extensions_reads |= bit; } if (address == f.base + 0x603 || address == f.base + 0x700) { expect(address == f.base + (f.taken ? 0x700 : 0x603) && !m_pipeline_drained_fetches, "RCD fetches only its selected continuation once"); ++m_pipeline_drained_fetches; } });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 276000;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_tc_return(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	unsigned const condition = variant % 2, tc = (variant / 2) % 2, producer = (variant / 4) % 3, gap = (variant / 12) % 2, selector = (variant / 24) % 2 ? 15 : 0;
	unsigned const words = producer == 2 ? 2 : producer;
	u32 const base = (variant / 48) % 2 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 16; ++i) program.write_word(base + 0x9000 + i, 0xf495);
	if (producer) { program.write_word(base + 0x9000, producer == 2 ? 0x34f8 : 0x3401); if (producer == 2) program.write_word(base + 0x9001, 0x06d1); }
	program.write_word(base + 0x9000 + words + gap, condition ? 0xfc30 : 0xfc20);
	program.write_word(base + 0x9001 + words + gap, 0xf4e1); program.write_word(base + 0x9100, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(0x2000, 0x9100);
	u16 const value = tc ? u16(1U << (15 - selector)) : 0;
	m_cpu->space(AS_DATA).write_word(producer == 2 ? 0x06d1 : 0x1201, value);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, 0xa824 | ((producer ? !tc : tc) ? 0x1000 : 0)}, {cpu_regs::STATE_ST1, 0xb00},
		{cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
		{cpu_regs::STATE_SP, 0x2000}, {cpu_regs::STATE_T, 0xa5f0 | selector}, {cpu_regs::STATE_A, 0xff12345678ULL},
		{cpu_regs::STATE_B, 0x81abcdef01ULL},
	});
	m_pipeline_operand_reads = m_fast_stack_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "tc_return_reads",
		[this, producer, value, tc, condition](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			if (at == 0x2000) { expect(tc == condition && !m_fast_stack_reads && word == 0x9100, "Only a taken TC/NTC return reads its stack target once"); ++m_fast_stack_reads; }
			else { expect(producer && !m_pipeline_operand_reads && at == (producer == 2 ? 0x06d1 : 0x1201) && word == value, "TC return consumes its organic BITT producer once"); ++m_pipeline_operand_reads; } });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "tc_return_no_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Conditional return never writes data memory"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = tc_return_phase;
	m_cpu->set_state_int(STATE_GENPC, base + 0x9000); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::bd_read_a_fixture tms320c54x_test_state::bd_read_a_case(unsigned v)
{
	bd_read_a_fixture f{};
	f.slot=v%2; v/=2; f.form=v%4; v/=4; f.pattern=v%4; v/=4;
	f.base=v%2 ? 0x20000 : 0; v/=2; f.source=(v%2 ? 0x20000 : 0)+0x2700; v/=2; f.snapshot=v;
	return f;
}

void tms320c54x_test_state::start_bd_read_a(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE,true); m_pipeline_peripheral_case=variant;
	auto const f=bd_read_a_case(variant); auto &program=m_cpu->space(AS_PROGRAM); auto &data=m_cpu->space(AS_DATA);
	for(unsigned i=0;i<16;++i) program.write_word(f.base+0x600+i,0xf495);
	program.write_word(f.base+0x600,0xf273); program.write_word(f.base+0x601,0x700); program.write_word(f.base+0x700,0xf4e1);
	if(f.slot) program.write_word(f.base+0x602,0x0084);
	program.write_word(f.base+0x602+f.slot,0x7e00|(f.form==0 || f.form==3 ? 0x20 : f.form==1 ? 0x83 : 0x93));
	program.write_word(f.source,f.value()); data.write_word(0x4020,0xdead); data.write_word(0x5100,5);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE,0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL,0); m_cpu->set_state_int(tms320c54x_device::STATE_PMST,0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0,0x1080); m_cpu->set_state_int(tms320c54x_device::STATE_ST1,0x900|(f.form==3 ? 0x4000 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR,0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IMR,0); m_cpu->set_state_int(tms320c54x_device::STATE_IFR,0);
	m_cpu->set_state_int(tms320c54x_device::STATE_A,f.source-5*f.slot); m_cpu->set_state_int(tms320c54x_device::STATE_B,0xfeabcd9876ULL); m_cpu->set_state_int(tms320c54x_device::STATE_T,0x5aa5);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP,0x4000); m_cpu->set_state_int(tms320c54x_device::STATE_AR3,0x4020); m_cpu->set_state_int(tms320c54x_device::STATE_AR4,0x5100);
	m_pipeline_operand_reads=m_pipeline_store_writes=m_port_reads=0;
	m_pipeline_fetch_tap=program.install_read_tap(f.source,f.source,"bd_read_a_source",[this,f](offs_t,u16 &word,u16) { if(machine().side_effects_disabled()) return; expect(word==f.value() && !m_pipeline_operand_reads++ && m_cpu->pipeline_cycles()==f.read()+4,"BD READA samples its retained program address once after the four held clocks"); auto const disable=machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.source,u16(~f.value())); });
	m_pipeline_operand_tap=data.install_read_tap(0,0xffff,"bd_read_a_data",[this,f](offs_t at,u16 &word,u16) { if(machine().side_effects_disabled()) return; expect(f.slot && at==0x5100 && word==5 && !m_port_reads++,"The first delay word supplies READA's address through ordinary ADD"); });
	m_pipeline_write_tap=data.install_write_tap(0,0xffff,"bd_read_a_write",[this,f](offs_t at,u16 &word,u16) { if(machine().side_effects_disabled()) return; expect(at==0x4020 && word==f.value() && !m_pipeline_store_writes++ && m_cpu->pipeline_cycles()==f.read()+5,"BD READA commits one retained program word to its original RAM destination"); });
	m_pipeline_replay=false; m_idle_fixture_tick=0; m_phase=bd_read_a_phase;
	m_cpu->set_state_int(STATE_GENPC,f.base+0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock()));
}






void tms320c54x_test_state::start_delayed_call_case(unsigned index)
{
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x0109d0, index < 4 ? 0xfb44 : 0xfb76); program.write_word(0x0109d1, 0x09e0);
	program.write_word(0x0109d2, 0xe800); // ANEQ must use A before this delay word.
	program.write_word(0x0109d3, 0xe901);
	program.write_word(0x0109d4, 0xf4e1);
	program.write_word(0x0109e0, 0xe902); program.write_word(0x0109e1, 0xfc00);
	m_cpu->space(AS_DATA).write_word(0x0fff, 0xbeef);
	set_registers(*m_cpu, {
		{STATE_GENPC, 0x0109d0}, {cpu_regs::STATE_SP, 0x1000}, {cpu_regs::STATE_ST1, 0x0900},
	});
	static constexpr u64 values[] = {0, 1, 0xffffffffffULL, 0x0100000000ULL, 0, 1, 0xffffffffffULL, 1, 0x0100000000ULL};
	set_registers(*m_cpu, {
		{cpu_regs::STATE_A, values[index]}, {cpu_regs::STATE_ST0, index >= 4 && index != 7 ? 0x0400 : 0},
		{cpu_regs::STATE_IDLE, 0},
	});
	m_phase = 3337 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_return_alt_case(unsigned index)
{
	static constexpr u64 values[] = {0xffffffffffULL, 0x8000000000ULL, 0, 1, 0x7fffffffffULL};
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x0109d0, 0xfc43); program.write_word(0x0109d1, 0xf4e1);
	program.write_word(0x0109e0, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(0x1000, 0x09e0);
	set_registers(*m_cpu, {
		{STATE_GENPC, 0x0109d0}, {cpu_regs::STATE_SP, 0x1000}, {cpu_regs::STATE_A, values[index]},
		{cpu_regs::STATE_ST1, 0x0800}, {cpu_regs::STATE_IDLE, 0},
	});
	m_phase = 2696 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

void tms320c54x_test_state::start_carry_branch_case(unsigned index)
{
	auto &program = m_cpu->space(AS_PROGRAM);
	unsigned cursor = 0x0109a0;
	program.write_word(cursor++, 0xf808 | ((index & 2) ? 4 : 0) | ((index & 4) ? 0x200 : 0));
	program.write_word(cursor++, 0x09b0);
	if (index & 4) { program.write_word(cursor++, 0xf495); program.write_word(cursor++, 0xf495); }
	program.write_word(cursor++, 0xf020); program.write_word(cursor++, 0x1111); program.write_word(cursor, 0xf4e1);
	program.write_word(0x0109b0, 0xf020); program.write_word(0x0109b1, 0x2222); program.write_word(0x0109b2, 0xf4e1);
	set_registers(*m_cpu, {
		{STATE_GENPC, 0x0109a0}, {cpu_regs::STATE_ST0, 0x02a5 | ((index & 1) ? 0x800 : 0)}, {cpu_regs::STATE_IDLE, 0},
	});
	m_phase = 1620 + index;
	m_check_timer->adjust(attotime::from_usec(100));
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

void tms320c54x_test_state::start_xc_barrier(unsigned index)
{
	unsigned const kind = index % 3;
	bool const initial = (index / 3) % 2;
	auto &program = m_cpu->space(AS_PROGRAM);
	program.write_word(0x600, initial ? 0xf4bc : 0xf5bc);
	program.write_word(0x601, kind == 0 ? 0xfc00 : kind == 1 ? 0xfc45 : 0xfc44);
	program.write_word(0x602, initial ? 0xfd20 : 0xfd30);
	program.write_word(0x603, 0xe933); program.write_word(0x604, 0xf495); program.write_word(0x605, 0xf4e1);
	m_cpu->space(AS_DATA).write_word(0x1000, 0x602);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_PC, 0x600}, {cpu_regs::STATE_SP, 0x1000}, {cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0},
		{cpu_regs::STATE_ST0, 0xe05 | (initial ? 0x1000 : 0)}, {cpu_regs::STATE_ST1, 0x800}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_IDLE, 0},
	});
	m_cpu->set_pipeline_frontend(index < 12);
	m_rom4_codec_replay = false; m_rom4_codec_replays = 0;
	m_phase = 524000 + index;
	if (!((index / 6) % 2))
	{
		expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save status producer and sequential-target return barrier");
		m_rom4_codec_replays = 1;
	}
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::xc_latency_fixture tms320c54x_test_state::xc_latency_case(unsigned index)
{
	unsigned const kind = (index / 16) % 6;
	return {index % 4, kind, 1 + (index / 96) % 2, kind >= 4 ? 2U : 1U,
		bool((index / 8) % 2), bool((index / 4) % 2), index < 384, bool((index / 192) % 2)};
}

void tms320c54x_test_state::start_xc_latency(unsigned index)
{
	auto const f = xc_latency_case(index);
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 20; ++i) program.write_word(0x600 + i, 0xf495);
	unsigned pc = 0x600;
	if (f.kind < 2) program.write_word(pc++, (f.initial ? 0xf4b0 : 0xf5b0) | (f.kind ? 11 : 12));
	else if (f.kind < 4) program.write_word(pc++, 0xe800 | ((f.kind & 1) << 8) | !f.initial);
	else { program.write_word(pc++, 0xf020 | ((f.kind & 1) << 8)); program.write_word(pc++, !f.initial); }
	pc += f.gap;
	u16 const predicate = f.kind == 0 ? (f.polarity ? 0x30 : 0x20) : f.kind == 1 ? (f.polarity ? 0x0c : 8) :
		0x40 | ((f.kind & 1) << 3) | (f.polarity ? 4 : 5);
	program.write_word(pc++, (f.slots == 2 ? 0xff00 : 0xfd00) | predicate);
	if (f.slots == 2)
		program.write_word(pc++, f.kind < 2 ? ((f.polarity ? 0xf4b0 : 0xf5b0) | (f.kind ? 11 : 12)) :
			0xe800 | ((f.kind & 1) << 8) | !f.polarity);
	program.write_word(pc++, f.kind >= 2 && !(f.kind & 1) ? 0xe933 : 0xe833);
	program.write_word(pc + 1, 0xf4e1);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_PC, 0x600}, {cpu_regs::STATE_SP, 0x1000},
		{cpu_regs::STATE_A, f.kind >= 2 && !(f.kind & 1) ? f.initial : 0},
		{cpu_regs::STATE_B, f.kind >= 2 && (f.kind & 1) ? f.initial : 0},
	});
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.status(false));
	set_registers(*m_cpu, {
		{cpu_regs::STATE_ST1, 0x800}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_TCR, 0x10},
		{cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_IDLE, 0},
	});
	m_cpu->set_pipeline_frontend(f.staged);
	m_rom4_codec_replay = false; m_rom4_codec_replays = 0;
	m_phase = 523000 + index;
	if (!f.snapshot)
	{
		expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save XC condition source before its producer");
		m_rom4_codec_replays = 1;
	}
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_xc_status(unsigned index)
{
	auto &program = m_cpu->space(AS_PROGRAM);
	unsigned const mode = index % 32, gap = mode % 4;
	bool const set = (mode / 4) & 1, initial = (mode / 8) & 1, tc = (mode / 16) & 1;
	for (unsigned i = 0; i < 16; ++i) program.write_word(0x600 + i, 0xf495);
	program.write_word(0x600, set ? 0xf5bc : 0xf4bc);
	program.write_word(0x601 + gap, tc ? 0xfd30 : 0xfd20);
	program.write_word(0x602 + gap, 0xe833);
	program.write_word(0x604 + gap, 0xf4e1);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_PC, 0x600}, {cpu_regs::STATE_SP, 0x1000}, {cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0},
		{cpu_regs::STATE_ST0, 0xe05 | (initial ? 0x1000 : 0)}, {cpu_regs::STATE_ST1, 0x800}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_IDLE, 0},
	});
	m_cpu->set_pipeline_frontend(index < 64);
	m_rom4_codec_replay = false; m_rom4_codec_replays = 0;
	m_phase = 522000 + index;
	if (!(index & 32))
	{
		expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save XC/status producer before Access sampling or Execute publication");
		m_rom4_codec_replays = 1;
	}
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_atomic_conditional_return(unsigned index)
{
	m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove();
	auto const f = conditional_register_case(index % 72);
	unsigned const condition = index % 12;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	program.write_word(0x600, 0xfc40 | (condition < 6 ? condition + 2 : condition + 4));
	program.write_word(0x601, 0xe811); program.write_word(0x602, 0xf4e1);
	program.write_word(0x700, 0xe822); program.write_word(0x701, 0xf4e1);
	data.write_word(0x1000, 0x700);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_PC, 0x600}, {cpu_regs::STATE_SP, 0x1000}, {cpu_regs::STATE_A, f.a}, {cpu_regs::STATE_B, f.b},
		{cpu_regs::STATE_ST0, 0x1e05}, {cpu_regs::STATE_ST1, 0x800}, {cpu_regs::STATE_PMST, 0},
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_IDLE, 0},
	});
	m_pipeline_operand_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "atomic_conditional_return_read", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		expect(f.take && at == 0x1000 && word == 0x700 && !m_pipeline_operand_reads++, "Atomic conditional return reads one stack word only when taken"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "atomic_conditional_return_write", [this](offs_t, u16 &, u16) {
		if (!machine().side_effects_disabled()) expect(false, "Atomic conditional return never writes the stack"); });
	m_rom4_codec_replay = false; m_rom4_codec_replays = 0;
	m_phase = 519000 + index;
	if (index < 72)
	{
		m_recurring_irq_snapshot = {0};
		expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save atomic return before predicate and optional stack read");
		m_rom4_codec_replays = 1;
	}
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_atomic_overflow_branch(unsigned index)
{
	auto &program = m_cpu->space(AS_PROGRAM);
	unsigned const flags = (index >> 3) & 3, accumulator = (index >> 1) & 1;
	program.write_word(0x600, 0xf860 | ((index & 1) << 9) | (accumulator << 3) | (((index >> 2) & 1) << 4));
	program.write_word(0x601, 0x700);
	program.write_word(0x602, 0xf5b0 | (10 - accumulator));
	program.write_word(0x603, 0xe955);
	program.write_word(0x604, 0xe811); program.write_word(0x605, 0xf4e1);
	program.write_word(0x700, 0xe822); program.write_word(0x701, 0xf4e1);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_PC, 0x600}, {cpu_regs::STATE_SP, 0x1000}, {cpu_regs::STATE_A, 0}, {cpu_regs::STATE_B, 0},
		{cpu_regs::STATE_ST0, 0x1805 | ((flags & 1) << 10) | ((flags & 2) << 8)}, {cpu_regs::STATE_ST1, 0x800},
		{cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_ILLEGAL, 0}, {cpu_regs::STATE_IDLE, 0},
	});
	m_rom4_codec_replay = false; m_rom4_codec_replays = 0;
	m_phase = 305000 + index;
	if (!(index & 32))
	{
		expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save overflow branch before predicate sampling");
		m_rom4_codec_replays = 1;
	}
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

bool tms320c54x_test_state::check_tc_return_results()
{
	if (m_phase == tc_return_phase)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, producer = (variant / 4) % 3, gap = (variant / 12) % 2;
		unsigned const offset = (producer == 2 ? 2 : producer) + gap, checkpoint = std::array<unsigned, 4>{3, 4, 5, 7}[variant / 96] + offset;
		bool const tc = (variant / 2) % 2, taken = tc == bool(variant % 2);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "RC TC/NTC and adjacent BITT are admitted");
		expect(bool(m_cpu->state_int(tms320c54x_device::STATE_IDLE)) == (tick >= (taken ? 11 : 9) + offset), "TC/NTC return retains Class 32's taken/untaken cadence");
		if (tick == 5 + offset)
		{
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x2000, 0xdead);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa824 | (!tc ? 0x1000 : 0));
		}
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_fast_stack_reads},
			"Save TC return around producer, condition sampling and retained target");
		if (tick == 32)
		{
			u32 const base = (variant / 48) % 2 ? 0x20000 : 0;
			expect(m_cpu->state_int(STATE_GENPC) == base + (taken ? 0x9101 : 0x9002 + offset) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x2000 + unsigned(taken) &&
				m_pipeline_operand_reads == unsigned(producer != 0) && m_fast_stack_reads == unsigned(taken) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0xa824 | (!tc ? 0x1000 : 0)) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0xb00 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == 0xff12345678ULL && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x81abcdef01ULL && m_cpu->state_int(tms320c54x_device::STATE_T) == (0xa5f0 | ((variant / 24) % 2 ? 15 : 0)), "TC/NTC return retains predicate and target, preserves registers and never touches an untaken stack");
			if (replay_pipeline_at(tc_return_phase + 1, checkpoint,
				"Restore TC return without duplicate producer or stack accesses")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 383) { start_tc_return(variant + 1); return true; }
			osd_printf_info("TMS320C54x TC returns: PASS cases=384 restore=384 predicates=TC,NTC outcomes=2 producers=status,BITT_short,BITT_absolute gaps=0,1 selectors=0,15 pages=2 snapshots=4 predicate_retained=1 target_retained=1 untaken_stack_reads=0 data_writes=0 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated TC returns complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == tc_return_phase + 1) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = tc_return_phase; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_bd_read_a_results()
{
	if (m_phase == bd_read_a_phase)
	{
		auto const f=bd_read_a_case(m_pipeline_peripheral_case); unsigned const tick=++m_idle_fixture_tick;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),"Short READA in BD delay spans is admitted");
		if(tick==f.read()) { expect(m_cpu->state_int(tms320c54x_device::STATE_AR3)==unsigned(f.form==2 ? 0x4021 : 0x4020),"READA owns exactly one destination update at Read"); m_cpu->set_state_int(tms320c54x_device::STATE_AR3,0x7000); }
		if(tick==f.read()+1) { expect(m_cpu->state_int(tms320c54x_device::STATE_A)==f.source,"READA's program address includes the older ADD result"); m_cpu->set_state_int(tms320c54x_device::STATE_A,0x5a5a); }
		if(tick==f.checkpoint() && !m_pipeline_replay) { m_recurring_irq_snapshot={m_pipeline_operand_reads,m_pipeline_store_writes,m_port_reads}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE,"Save BD READA before or across retained program transfer"); }
		if(tick==32)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC)==f.base+0x701 && m_cpu->state_int(tms320c54x_device::STATE_SP)==0x4000 && m_cpu->state_int(tms320c54x_device::STATE_A)==0x5a5a && m_cpu->state_int(tms320c54x_device::STATE_B)==0xfeabcd9876ULL && m_cpu->state_int(tms320c54x_device::STATE_T)==0x5aa5 && m_cpu->state_int(tms320c54x_device::STATE_ST0)==0x1080 && m_cpu->state_int(tms320c54x_device::STATE_ST1)==(0x900|(f.form==3 ? 0x4000 : 0)),"BD READA retains the branch path, program source, modes and sibling registers");
			expect(m_pipeline_operand_reads==1 && m_pipeline_store_writes==1 && m_port_reads==f.slot && m_cpu->state_int(tms320c54x_device::STATE_AR3)==0x7000 && m_cpu->state_int(tms320c54x_device::STATE_AR4)==0x5100,"READA neither resamples poisoned addresses nor duplicates transfers");
			if(!m_pipeline_replay) { m_pipeline_replay=true; m_idle_fixture_tick=f.checkpoint(); m_pipeline_operand_reads=m_recurring_irq_snapshot[0]; m_pipeline_store_writes=m_recurring_irq_snapshot[1]; m_port_reads=m_recurring_irq_snapshot[2]; m_cpu->set_pipeline_frontend(true); return restore_pipeline_at(bd_read_a_phase+1, "Restore BD READA after clearing future pipeline state"); }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if(m_pipeline_peripheral_case<511) { start_bd_read_a(m_pipeline_peripheral_case+1); return true; }
			osd_printf_info("TMS320C54x BD READA: PASS cases=512 restore=512 slots=2 forms=DP,plain,postinc,SP values=4 caller_pages=2 source_pages=2 snapshots=4 older_ADD=slot1 retained_PAR=1 retained_destination=1 source_poison=1 reset_before_restore=1 physical_phase_claim=0\n"); throw emu_fatalerror(3,"TMS320C54x isolated BD READA complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true;
	}
	if(m_phase==bd_read_a_phase+1) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase=bd_read_a_phase; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_carry_branch_results()
{
	if (m_phase == 1619)
	{
		expect(m_cpu->state_int(tms320c54x_device::STATE_B) == 0xc800 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x4800 &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE),
			"RPTB publishes bit-15 BRAF while preserving independent bit-14 CPL on retirement");
		osd_printf_info("TMS320C54x BRAF/CPL independence: PASS\n");
		start_carry_branch_case(0);
		return true;
	}
	if (m_phase >= 1620 && m_phase < 1628)
	{
		unsigned const index = m_phase - 1620;
		bool const taken = bool(index & 1) == bool(index & 2);
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (taken ? 0x2222 : 0x1111) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0x02a5 | ((index & 1) ? 0x800 : 0)) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE), "BC/BCD C/NC uses carry and preserves status");
		if (index < 7) { start_carry_branch_case(index + 1); return true; }
		osd_printf_info("TMS320C54x carry branch conformance: PASS variants=8\n");
		start_cmpm_offset_case(0);
		return true;
	}
	return false;
}

bool tms320c54x_test_state::check_return_alt_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 2696 && m_phase < 2701)
	{
		unsigned const index = m_phase - 2696;
		bool const taken = index < 2;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == (taken ? 0x1001 : 0x1000) &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == (taken ? 0x09e1 : 0x09d2) &&
			m_cpu->state_int(tms320c54x_device::STATE_XPC) == 1,
			"RC ALT tests signed 40-bit A, consumes only a taken return and preserves XPC");
		if (index < 4) { start_return_alt_case(index + 1); return true; }
		osd_printf_info("TMS320C54x RC ALT conformance: PASS variants=5\n");
		start_software_interrupt_case(0);
		return true;
	}
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

bool tms320c54x_test_state::check_delayed_call_results()
{
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 3337 && m_phase < 3346)
	{
		unsigned const index = m_phase - 3337;
		bool const taken = index == 1 || index == 2 || index == 3 || index == 5 || index == 8;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == 0 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (taken ? 2 : 1) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 &&
			data.read_word(0x0fff) == (taken ? 0x09d4 : 0xbeef) &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x09d5 &&
			m_cpu->state_int(tms320c54x_device::STATE_XPC) == 1,
			"CCD captures full-width A and combined overflow conditions, executes delay words and returns after them without changing XPC");
		if (index < 8) { start_delayed_call_case(index + 1); return true; }
		osd_printf_info("TMS320C54x delayed conditional call conformance: PASS variants=9\n");
		start_asm_store_case(0);
		return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_far_return_results()
{
	if (m_phase == 251290)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		unsigned const form = m_pipeline_peripheral_case % 36, prefix = m_pipeline_peripheral_case >= 36;
		bool const delayed = form >= 12;
		bool const long_delay = form >= 24;
		unsigned const first_read = (delayed ? 4 : 5) + prefix, target_fetch = (delayed ? 6 : 8) + prefix, target_execute = (delayed ? 10 : 12) + prefix;
		u64 const checkpoint = first_read - 1 + (m_pipeline_peripheral_case % 6) / 2;
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_operand_reads == unsigned(cycle >= first_read) + unsigned(cycle > first_read) &&
			m_pipeline_extensions_reads == unsigned(cycle >= target_fetch), "Far return retains both stack samples and fetches the target once across replay");
		expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 - prefix + unsigned(prefix && cycle >= 5) + unsigned(cycle >= first_read) + unsigned(cycle > first_read) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR0 + 1) == (prefix && cycle >= 5 ? 0x42 : 0) &&
			bool(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x800) == (form % 12 >= 6 || cycle < (delayed ? 6 : 7) + prefix) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (long_delay ? (cycle >= 8 + prefix ? 0x1234 : 0) : delayed && cycle >= 7 + prefix ? 0x11 : 0) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (cycle >= target_execute ? 0x55 : delayed && !long_delay && cycle >= 8 + prefix ? 0x22 : 0), "Far return preserves older POPM, its INTM contract and exactly its two delay words");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save before or between retained far-return stack accesses");
		}
		if (cycle == target_execute)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true;
				m_pipeline_operand_reads = unsigned(checkpoint >= first_read) + unsigned(checkpoint > first_read); m_pipeline_extensions_reads = 0;
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Restore far-return stack samples and pending extra clock");
				return restart_pipeline_observer(251300, checkpoint + 1);
			}
			m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case < 71) { start_pipeline_far_return(m_pipeline_peripheral_case + 1); return true; }
			osd_printf_info("TMS320C54x staged far return: PASS cases=72 plain=24 delayed=48 multiword_delay=24 preceding_popm=36 restore=72 duplicate_reads=0 silicon_phase_claim=0\n");
			start_pipeline_far_irq(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251300)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251290;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_conditional_branch_results()
{
	if (m_phase == 251610)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		unsigned const variant = m_pipeline_peripheral_case;
		bool const b = BIT(variant, 0), eq = BIT(variant, 1), nonzero = BIT(variant, 2), taken = eq ? !nonzero : nonzero;
		u64 const initial = nonzero ? 0x0100000000ULL : 0, opposite = nonzero ? 0 : 0x0100000000ULL;
		bool const memory_dp = variant >= 48;
		u64 const checkpoint = memory_dp ? std::array<unsigned, 3>{6, 8, 11}[(variant / 16) % 3] : std::array<unsigned, 3>{4, 6, 8}[variant / 16];
		if (cycle == 6) m_cpu->set_state_int(b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A, opposite);
		u64 const expected_a = memory_dp ? (taken && cycle >= 14 ? 0x3333 : !b && cycle >= 6 ? opposite : b ? 0x9999 : initial) :
			cycle >= 8 ? 0x11 : !b && cycle >= 6 ? opposite : b ? 0x9999 : initial;
		u64 const expected_b = memory_dp ? (taken && cycle >= 15 ? 0x55 : !taken && cycle >= 13 ? 0x66 : b && cycle >= 6 ? opposite : b ? initial : 0x7777) :
			taken && cycle >= 12 ? 0x55 : !taken && cycle >= 11 ? 0x66 : cycle >= 10 ? 0x44 : b && cycle >= 6 ? opposite : b ? initial : 0x7777;
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_operand_reads == unsigned(taken && cycle >= (memory_dp ? 10 : 8)) &&
			m_pipeline_extensions_reads == unsigned(cycle >= 4) + unsigned(cycle >= 6) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == expected_a && m_cpu->state_int(tms320c54x_device::STATE_B) == expected_b &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (memory_dp && cycle >= 10 ? 0xa612 : 0xa608) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800 &&
			(!memory_dp || (m_pipeline_drained_fetches == unsigned(cycle >= 7) && m_pipeline_store_writes == unsigned(taken && cycle >= 13))),
			"BCD retains the sampled 40-bit zero predicate, executes both delays and chooses only its proper continuation");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save BCD bubble, sampled predicate or pending delay retirement");
		}
		if (cycle == 18)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_operand_reads = unsigned(taken && checkpoint >= (memory_dp ? 10 : 8));
				m_pipeline_extensions_reads = unsigned(checkpoint >= 4) + unsigned(checkpoint >= 6);
				m_pipeline_drained_fetches = unsigned(memory_dp && checkpoint >= 7); m_pipeline_store_writes = 0;
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Restore BCD target/predicate ownership without duplicate fetches");
				return restart_pipeline_observer(251620, checkpoint + 1);
			}
			m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 95) { start_pipeline_conditional_branch(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged conditional branches: PASS cases=96 conditions=AEQ,ANEQ,BEQ,BNEQ pages=2 outcomes=2 memory_dp_delay=48 restore=96 duplicate_fetches=0 silicon_phase_claim=0\n");
			start_pipeline_plain_return(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251620)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251610;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251630)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		bool const pushed = m_pipeline_peripheral_case >= 12, adjacent = pushed && m_pipeline_peripheral_case < 24;
		unsigned const prefix = pushed ? (adjacent ? 1 : 2) : 0;
		bool const enable = BIT(m_pipeline_peripheral_case, 1);
		u64 const checkpoint = 3 + prefix + (m_pipeline_peripheral_case / 4) % 3;
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_operand_reads == unsigned(cycle >= 5 + prefix) &&
			m_pipeline_extensions_reads == unsigned(cycle >= 7 + prefix) &&
			m_pipeline_store_writes == unsigned(pushed && cycle >= 6) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == (!pushed && cycle >= 4 ? 0x1001 : prefix == 2 && cycle == 5 ? 0xfff : 0x1000) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa61b &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == (enable && cycle >= 6 + prefix ? 0 : 0x800) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (cycle >= 11 + prefix ? (adjacent ? 0x99 : 0x55) : 0),
			"Plain return owns one pop/read/target fetch, preserves RET's INTM and clears RETE only at retirement");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save plain return before/after Access or stack Read");
		}
		if (cycle == 14)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true;
				m_pipeline_operand_reads = unsigned(checkpoint >= 5 + prefix); m_pipeline_extensions_reads = 0;
				m_pipeline_store_writes = unsigned(pushed && checkpoint >= 6);
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Restore ordinary return and adjacent push ownership without duplicate bus operations");
				return restart_pipeline_observer(251640, checkpoint + 1);
			}
			m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case < 35) { start_pipeline_plain_return(m_pipeline_peripheral_case + 1); return true; }
			osd_printf_info("TMS320C54x staged plain returns: PASS cases=36 forms=RET,RETE pages=2 adjacent_old_word=12 nop_new_word=12 restore=36 duplicate_accesses=0\n");
			start_pipeline_call(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251640)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251630;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251650)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		bool const t_delay = m_pipeline_peripheral_case >= 24;
		u16 const parameter = t_delay ? std::array<u16, 9>{6, 0, 0x8000, 0xf495, 0xffff, 0x76e6, 0x4ee6, 0x7001, 0x54e6}[(m_pipeline_peripheral_case - 24) / 12] : 0;
		unsigned const adjacent = t_delay ? BIT(m_pipeline_peripheral_case, 1) : m_pipeline_peripheral_case >= 12;
		bool const delayed = t_delay || BIT(m_pipeline_peripheral_case, 1);
		u16 const continuation = 0x600 + adjacent + (delayed ? 4 : 2);
		u64 const checkpoint = 4 + adjacent + (m_pipeline_peripheral_case / 4) % 3;
		u64 const sp = cycle < 4 + adjacent ? 0x1000 : cycle >= 9 + adjacent ? 0x1000 - adjacent : 0xfff - adjacent;
		u64 const expected_b = cycle >= 16 + adjacent ? 0x66 : cycle >= 10 + adjacent ? (t_delay ? parameter : 0x55) : delayed && !t_delay && cycle >= 9 + adjacent ? 0x22 : 0;
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_store_writes == unsigned(cycle >= 6 + adjacent) &&
			m_pipeline_drained_fetches == unsigned(adjacent && cycle >= 6) &&
			m_pipeline_operand_reads == unsigned(cycle >= 10 + adjacent) &&
			m_pipeline_extensions_reads == unsigned(cycle >= 6 + adjacent) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == sp &&
			m_cpu->state_int(tms320c54x_device::STATE_RTN) == (cycle >= 5 + adjacent ? continuation : 0x8888) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (delayed && !t_delay && cycle >= 8 + adjacent ? 0x11 : 0) &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == (t_delay && cycle >= 7 + adjacent ? parameter : 0xbeef) &&
			m_pipeline_register_writes == unsigned(t_delay && cycle >= 7 + adjacent) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == expected_b,
			"CALL owns its stack/RTN phases, executes only delay slots and returns to the correct continuation");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save CALL after allocation/RTN/write ownership");
		}
		if (cycle == 22)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true;
				m_pipeline_store_writes = m_pipeline_extensions_reads = unsigned(checkpoint >= 6 + adjacent);
				m_pipeline_drained_fetches = unsigned(adjacent && checkpoint >= 6); m_pipeline_operand_reads = 0;
				m_pipeline_register_writes = 0;
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Restore retained CALL target and return word without duplicate stack writes");
				return restart_pipeline_observer(251660, checkpoint + 1);
			}
			m_pipeline_write_tap.remove(); m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case < 131) { start_pipeline_call(m_pipeline_peripheral_case + 1); return true; }
			osd_printf_info("TMS320C54x staged calls: PASS cases=132 forms=CALL,CALLD pages=2 adjacent_push=66 stm_t_delay=108 parameters=9 restore=132 duplicate_stack_writes=0\n");
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251660)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251650;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251690)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		bool const composed = m_pipeline_peripheral_case >= 4;
		bool const branch = m_pipeline_peripheral_case >= 8;
		bool const nonzero = BIT(m_pipeline_peripheral_case, 0);
		u64 const checkpoint = branch ? 11 + (m_pipeline_peripheral_case - 8) / 2 : 3 + 2 * (m_pipeline_peripheral_case & 3);
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_store_writes == unsigned(cycle >= 7) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == (cycle >= 6 ? 0x4ff : 0x500) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == (cycle >= 9 ? 0x800 : 0),
			"Original unconditional XC worker prefix preserves PSHM/INTM ordering");
		if (composed)
			expect(m_pipeline_operand_reads == unsigned(cycle >= 9) + unsigned(cycle >= 11) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR0) == (cycle >= 9 ? 0xbeef : 0x5678) &&
				m_cpu->state_int(tms320c54x_device::STATE_AR2) == (cycle >= 10 ? 0x512 : cycle >= 9 ? 0x511 : 0x510) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == (cycle >= 12 ? (branch ? (cycle >= 14 ? !nonzero : nonzero) : 0x1357) : 0x1234),
				"Original worker prefix composes XC, stack, INTM, MVDK and next indirect load without duplicate source updates");
		if (branch)
			expect(m_pipeline_extensions_reads == unsigned(nonzero && cycle >= 14) &&
				m_pipeline_drained_fetches == unsigned(!nonzero && cycle >= 14), "Original worker BC retains its page-2 target and sampled route on replay");
		if (branch && cycle == 13) m_cpu->set_state_int(tms320c54x_device::STATE_A, !nonzero);
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save original XC prefix");
		}
		if (composed && cycle == 9) m_cpu->space(AS_DATA).write_word(0x510, 0);
		if (branch && cycle == 11) m_cpu->space(AS_DATA).write_word(0x511, 0xdead);
		if (cycle == (branch ? 21 : 16))
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_store_writes = unsigned(checkpoint >= 7);
				m_pipeline_operand_reads = composed ? unsigned(checkpoint >= 9) + unsigned(checkpoint >= 11) : 0;
				m_pipeline_extensions_reads = m_pipeline_drained_fetches = 0;
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Restore original XC prefix without duplicate push");
				return restart_pipeline_observer(251700, checkpoint + 1);
			}
			m_pipeline_write_tap.remove(); m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (m_pipeline_peripheral_case < 13) { start_pipeline_worker_xc(m_pipeline_peripheral_case + 1); return true; }
			osd_printf_info("TMS320C54x staged original worker XC prefix: PASS cases=14 restore=14 composed_seven_words=4 composed_nine_words=6 duplicate_pushes=0\n");
			start_pipeline_xc_irq(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251700)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251690;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_xc_tc_results()
{
	if (m_phase == 251730)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		unsigned const variant = m_pipeline_peripheral_case;
		bool const producer = variant >= 64;
		unsigned const gap = producer ? (variant - 64) / 16 : 0;
		unsigned const offset = producer ? 1 + gap : 0;
		u64 const checkpoint = producer ? 4 + gap + BIT(variant, 3) : std::array<unsigned, 4>{3, 4, 6, 8}[(variant / 8) & 3];
		bool const sample = producer && gap < 2 ? BIT(variant, 0) : !BIT(variant, 0);
		bool const execute = sample == BIT(variant, 2);
		auto const writes_at = [execute, variant, offset, producer](u64 clock) {
			if (variant >= 32 && !producer) return unsigned(execute && clock >= 8) + unsigned(clock >= 9);
			return unsigned(execute && clock >= 7 + offset) + unsigned((execute || !BIT(variant, 1)) && clock >= 8 + offset);
		};
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_pipeline_store_writes == writes_at(cycle), "XC TC/NTC preserves accepted/rejected slot ownership");
		if (cycle == 3 && !producer) m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa60e | (!BIT(variant, 0) << 12));
		if (cycle == 4 && !producer) m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xa60e | (BIT(variant, 0) << 12));
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save XC before/after Access sampling or slot retirement");
		}
		if (cycle == 12)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_store_writes = writes_at(checkpoint);
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Restore XC decision and pending rejected-word count");
				return restart_pipeline_observer(251740, checkpoint + 1);
			}
			m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 127) { start_pipeline_xc_tc(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged XC TC sampling: PASS cases=128 forms=TC,NTC words=1,2 restore=128 two_word_slots=32 producer_cases=64 access_sampled=1 rejected_writes=0\n");
			if (profile_is("xc-controls"))
			{
				osd_printf_info("TMS320C54x staged XC controls: PASS unconditional_irq=4 arithmetic_irq=32 tc=128 restore=164\n");
				throw emu_fatalerror(0, "TMS320C54x isolated XC controls complete");
			}
			start_pipeline_plain_branch(); return true;
		}
		++m_pipeline_expected_cycle; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251740)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251730;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251750)
	{
		u64 const cycle = m_cpu->pipeline_cycles();
		unsigned const variant = m_pipeline_peripheral_case;
		bool const taken = BIT(variant, 1) ? !BIT(variant, 2) : BIT(variant, 2);
		u64 const checkpoint = 4 + variant / 16;
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_operand_reads == unsigned(taken && cycle >= 7) &&
			m_pipeline_extensions_reads == unsigned(cycle >= 4) + unsigned(cycle >= 6) &&
			m_pipeline_store_writes == (taken ? 0 : unsigned(cycle >= 8) + unsigned(cycle >= 9)),
			"BC retains sequential Fetch but suppresses taken-path data effects");
		if (cycle == 6) m_cpu->set_state_int(BIT(variant, 0) ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A, BIT(variant, 2) ? 0 : 1);
		if (cycle >= 11) expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (taken ? 0x55 : 0x66), "BC uses retained outcome despite accumulator changes after evaluation");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save BC before/after held sequential word or decision");
		}
		if (cycle == 14)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_operand_reads = m_pipeline_store_writes = 0;
				m_pipeline_extensions_reads = unsigned(checkpoint >= 4) + unsigned(checkpoint >= 6);
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Restore BC held word and retained route without repeated Fetch");
				return restart_pipeline_observer(251760, checkpoint + 1);
			}
			m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 47) { start_pipeline_plain_branch(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged plain conditional branches: PASS cases=48 conditions=AEQ,ANEQ,BEQ,BNEQ pages=2 outcomes=2 restore=48 duplicate_fetches=0 taken_data_reads=0\n");
			if (profile_is("plain-branch")) throw emu_fatalerror(0, "TMS320C54x isolated plain conditional branches complete");
			start_pipeline_delayed_return(); return true;
		}
		++m_pipeline_expected_cycle; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251760)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251750;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_delayed_return_results()
{
	if (m_phase == 251790)
	{
		u64 const cycle = m_cpu->pipeline_cycles(); unsigned const variant = m_pipeline_peripheral_case;
		bool const xc = variant >= 12, two_word = !xc && BIT(variant, 1);
		bool const irq = xc && !BIT(variant, 1);
		u64 const checkpoint = xc ? 6 + (variant - 12) / 4 : 4 + variant / 4;
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_operand_reads == unsigned(cycle >= 5) && m_pipeline_extensions_reads == unsigned(cycle >= 7) &&
			m_pipeline_store_writes == unsigned(cycle >= 3) + unsigned(cycle >= 4) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == (irq && cycle >= 13 ? 0x1000 : cycle >= 4 ? 0x1001 : 0x1000) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == (irq && cycle >= 10 && cycle < 14 ? 0 : 0x800) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (xc ? (BIT(variant, 1) ? 0x800 : 0) : !two_word && cycle >= 9 ? 0x11 : 0) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (cycle >= 11 ? 0x55 : !xc && !two_word && cycle >= 10 ? 0x44 : 0) &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == (two_word && cycle >= 8 ? 6 : 0),
			"RETD retains its pop and two words, preserves INTM and executes each delay before the target");
		if (xc && cycle == 6) m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 1);
		if (xc) expect(m_cpu->state_int(tms320c54x_device::STATE_IFR) == (irq && cycle >= 14 ? 0 : cycle >= 6 ? 1 : 0), "XC in RETD protects the resumed target before a pending IRQ is acknowledged");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save RETD queued delay words and return sample");
		}
		if (cycle == 15)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_operand_reads = unsigned(checkpoint >= 5); m_pipeline_extensions_reads = unsigned(checkpoint >= 7);
				m_pipeline_store_writes = 2;
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Restore RETD without repeated stack or delay Fetch");
				return restart_pipeline_observer(251800, checkpoint + 1);
			}
			m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_pipeline_write_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 23) { start_pipeline_delayed_return(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged delayed returns: PASS cases=24 pages=2 delay_forms=3 restore=24 xc_irq_cases=12 duplicate_stack_reads=0 third_delay_fetches=0\n");
			start_pipeline_addm(); return true;
		}
		++m_pipeline_expected_cycle; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251800)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251790;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}


bool tms320c54x_test_state::check_pipeline_retd_data_order_results()
{
	if (m_phase == 251910)
	{
		u64 const cycle = m_cpu->pipeline_cycles(); unsigned const variant = m_pipeline_peripheral_case;
		unsigned const prefix = BIT(variant, 1) ? 2 : 1; u64 const checkpoint = 4 + prefix + variant / 4;
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_operand_reads == unsigned(cycle >= 5 + prefix) && m_pipeline_store_writes == unsigned(cycle >= 6) &&
			m_pipeline_extensions_reads == unsigned(cycle >= 7 + prefix) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == (prefix == 2 && cycle == 5 ? 0xfff : 0x1000) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800 &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (cycle >= 11 + prefix ? (prefix == 1 ? 0x99 : 0x55) : 0),
			"RETD retains read-first DARAM target without changing pop, delays or interrupt mask semantics");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save RETD before/after same-address read/write ordering");
		}
		if (cycle == 15)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_operand_reads = unsigned(checkpoint >= 5 + prefix); m_pipeline_store_writes = unsigned(checkpoint >= 6); m_pipeline_extensions_reads = 0;
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Restore RETD target without late forwarding or duplicate stack access");
				return restart_pipeline_observer(251920, checkpoint + 1);
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 11) { start_pipeline_retd_data_order(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged RETD DARAM ordering: PASS cases=12 adjacent_old_word=6 nop_new_word=6 restore=12 duplicate_accesses=0\n");
			start_pipeline_data_pop(); return true;
		}
		++m_pipeline_expected_cycle; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 251920)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 251910;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_far_branch_results()
{
	if (m_phase == 252090)
	{
		unsigned const variant = m_pipeline_peripheral_case;
		unsigned const target_page = std::array<unsigned, 5>{0, 1, 2, 3, 0x7f}[variant / 8];
		unsigned const source_page = BIT(variant, 0) ? 2 : 0;
		unsigned const checkpoint = std::array<unsigned, 4>{3, 5, 7, 10}[(variant % 8) / 2];
		u64 const cycle = m_cpu->pipeline_cycles();
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_operand_reads == unsigned(cycle >= 3) && m_pipeline_extensions_reads == unsigned(cycle >= 6) &&
			// Fetch exposes the next page before retirement; that intermediate
			// debugger value is not an asserted architectural commit phase.
			(cycle == 6 || m_cpu->state_int(tms320c54x_device::STATE_XPC) == (cycle >= 7 ? target_page : source_page)) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (cycle >= 10 ? 0x44 : 0) && m_cpu->state_int(tms320c54x_device::STATE_B) == 0 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800,
			"FB commits the explicit target page and flushes sequential instructions without call semantics");
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save FB partial target decode, redirected fetch and page commitment");
		}
		if (cycle == 14)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_operand_reads = unsigned(checkpoint >= 3); m_pipeline_extensions_reads = unsigned(checkpoint >= 6);
				expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Restore FB page and literal without duplicate fetch or fallthrough");
				return restart_pipeline_observer(252100, checkpoint + 1);
			}
			m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 39) { start_pipeline_far_branch(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged FB: PASS cases=40 source_pages=2 target_pages=5 restore=40 duplicate_literal_fetches=0 duplicate_target_fetches=0\n");
			start_pipeline_absolute_load(); return true;
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252100)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252090;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_pipeline_far_call_results()
{
	if (m_phase == 252170)
	{
		unsigned const variant = m_pipeline_peripheral_case;
		unsigned const source_page = BIT(variant, 0) ? 2 : 0;
		unsigned const checkpoint = std::array<unsigned, 8>{3, 4, 5, 6, 7, 10, 11, 12}[(variant % 16) / 2];
		u64 const cycle = m_cpu->pipeline_cycles();
		auto const writes_at = [](u64 clock) { return unsigned(clock >= 6) + unsigned(clock >= 7); };
		auto const reads_at = [](u64 clock) { return unsigned(clock >= 10) + unsigned(clock >= 11); };
		auto const caller_fetches = [](u64 clock) { return unsigned(clock >= 4) + unsigned(clock >= 13); };
		expect(cycle == m_pipeline_expected_cycle && !m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_pipeline_store_writes == writes_at(cycle) && m_fast_stack_reads == reads_at(cycle) &&
			m_pipeline_operand_reads == unsigned(cycle >= 3) && m_pipeline_extensions_reads == unsigned(cycle >= 6) && m_pipeline_drained_fetches == caller_fetches(cycle) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == (cycle >= 11 ? 0x1000 : cycle >= 10 ? 0xfff : cycle >= 5 ? 0xffe : cycle >= 4 ? 0xfff : 0x1000) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (cycle >= 18 ? 0x11 : cycle >= 17 ? 0x44 : 0) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (cycle >= 10 ? 0x55 : 0) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0xa608 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800 &&
			m_cpu->state_int(tms320c54x_device::STATE_RTN) == 0xbeef,
			"FCALL and FRET balance their two-word frame and execute only the callee then real caller continuation");
		if (cycle >= 12) expect(m_cpu->state_int(tms320c54x_device::STATE_XPC) == source_page, "FRET restores the held source page even after target-page fetches");
		if (cycle == 10 || cycle == 11)
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_DATA).write_word(cycle == 10 ? 0xffe : 0xfff, 0x7fff);
		}
		if (cycle == checkpoint && !m_pipeline_replay)
		{
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save FCALL's partial frame and FRET's retained caller operands");
		}
		if (cycle == 20)
		{
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_pipeline_store_writes = writes_at(checkpoint); m_fast_stack_reads = reads_at(checkpoint);
				m_pipeline_operand_reads = unsigned(checkpoint >= 3); m_pipeline_extensions_reads = unsigned(checkpoint >= 6); m_pipeline_drained_fetches = caller_fetches(checkpoint);
				expect(restore_snapshot(machine(), *m_check_timer, m_saved_pipeline) == STATERR_NONE, "Restore far call/return without duplicated pushes, reads or continuation execution");
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_expected_cycle = checkpoint + 1; m_phase = 252180;
				m_check_timer->enable(true); return true;
			}
			m_pipeline_write_tap.remove(); m_pipeline_operand_tap.remove(); m_pipeline_fetch_tap.remove(); m_vector_prefetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 79) { start_pipeline_far_call(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged FCALL/FRET: PASS cases=80 source_pages=2 target_pages=5 restore=80 stack_words=2 duplicate_stack_writes=0 duplicate_stack_reads=0\n");
		}
		++m_pipeline_expected_cycle;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252180)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252170;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
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

bool tms320c54x_test_state::check_delayed_condition_return_results()
{
	if (m_phase == 276000)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = delayed_condition_return_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "RCD signed accumulator and TC predicates are admitted outside block and delay contexts");
		capture_pipeline_at(f.checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_pipeline_extensions_reads, &m_pipeline_drained_fetches},
			"Save RCD predicate stack sample and retained delay words");
		if (tick == 96)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + (f.taken ? 0x706 : 0x609) && m_cpu->state_int(tms320c54x_device::STATE_SP) == f.sp + unsigned(f.taken) && m_cpu->state_int(tms320c54x_device::STATE_A) == f.final_a && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.taken ? 0x55 : 0x33) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.final_st0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x900 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5aa5 && m_cpu->state_int(tms320c54x_device::STATE_AR2) == (f.layout ? 0xf4e1 : 0x1234), "RCD retains its predicate and target while both delay words retire and only taken paths pop");
			expect(m_pipeline_operand_reads == unsigned(f.taken) && m_pipeline_store_writes == 2 && m_pipeline_extensions_reads == 3 && m_pipeline_drained_fetches == 1, "RCD has no untaken stack access no duplicate delay fetch and exactly one continuation");
			if (replay_pipeline_at(276010, f.checkpoint,
				"Restore RCD without repeated stack or delay transactions")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 4479) { start_delayed_condition_return(variant + 1); return true; }
			osd_printf_info("TMS320C54x conditional delayed returns: PASS cases=4480 restore=4480 predicates=14 patterns=8 delay_layouts=2 stack_alignments=2 pages=2 snapshots=5 predicate_before_delay=1 taken_stack_reads=1 untaken_stack_reads=0 retained_target=1 delay_fetches=once interrupt_block_composition=unvalidated physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated conditional delayed returns complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 276010) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 276000; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_signed_xc_results()
{
	if (m_phase == 254880)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		auto const f = signed_xc_case(variant);
		unsigned const checkpoint = std::array<unsigned, 4>{3, 4, 5, 8}[variant / 576];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x060e && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5aa5, "Signed XC preserves legal ARP0 status and T");
		if (tick == 3 || tick == 4) m_cpu->set_state_int(f.b ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A, tick == 3 ? f.value : f.opposite);
		capture_pipeline_at(checkpoint, {&m_pipeline_store_writes, &m_pipeline_extensions_reads},
			"Save signed XC before/after predicate and protected-slot decisions");
		if (tick == 16)
		{
			unsigned const writes = unsigned(f.taken) + unsigned(f.taken || f.layout != 1);
			u16 const opcode = 0xfd42 + f.condition + (f.b ? 8 : 0) + (f.layout ? 0x200 : 0);
			expect_opcode(opcode, m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_store_writes == writes && m_pipeline_extensions_reads == 3 && m_cpu->state_int(tms320c54x_device::STATE_A) == (f.b ? 0x1234 : f.opposite) && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.b ? f.opposite : 0x3456), "Signed XC uses a retained 40-bit comparison, legal slot layout and no duplicate effects");
			if (replay_pipeline_at(254890, checkpoint,
				"Restore signed XC without resampling predicate or decoding rejected literal")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 2303) { start_signed_xc(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged signed XC: PASS cases=2304 restore=2304 conditions=GE,LT,NE,EQ,GT,LE accumulators=A,B patterns=8 layouts=XC1_single,XC2_two_single,XC2_one_double pages=2 snapshots=4 predicate=Access rejected_writes=0 opcode_literal=1 signed_irq_composition=unvalidated\n");
			throw emu_fatalerror(0, "TMS320C54x isolated signed XC complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254890) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254880; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_xc_barrier_results()
{
	if (m_phase >= 524000 && m_phase < 524024)
	{
		unsigned const index = m_phase - 524000;
		bool const initial = (index / 3) % 2, taken = index % 3 != 2;
		if (!m_rom4_codec_replays && m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x602)
		{
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save return barrier after its optional stack read");
			m_rom4_codec_replays = 1;
		}
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Return barrier and XC remain admitted");
		if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE)) { m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
		if (m_cpu->state_int(tms320c54x_device::STATE_B) != 0x33)
			osd_printf_info("XC return barrier divergence: case=%u staged=%u kind=%u initial_tc=%u B=%04x expected_B=0033\n", index, index < 12, index % 3, initial,
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_B)));
		expect(m_rom4_codec_replays == 1 && m_cpu->state_int(tms320c54x_device::STATE_A) == 0 && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x33 &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x606 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 + unsigned(taken) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0xe05 | (!initial ? 0x1000 : 0)),
			"Return pipeline drain makes preceding status visible even at a sequential target or untaken conditional return");
		if (!m_rom4_codec_replay)
		{
			expect(restore_snapshot(machine(), *m_check_timer, m_saved_pipeline) == STATERR_NONE, "Restore return drain and selected XC without reusing stale history");
			m_phase = 524000 + index; m_rom4_codec_replay = true;
			m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
		}
		if (index == 11) osd_printf_info("TMS320C54x staged XC return barriers: PASS cases=12 restore=12 forms=RET,RC_taken,RC_untaken sequential_target=1 snapshots=2 physical_phase_claim=0\n");
		if (index < 23) { start_xc_barrier(index + 1); return true; }
		osd_printf_info("TMS320C54x atomic XC return barriers: PASS cases=12 restore=12 forms=RET,RC_taken,RC_untaken sequential_target=1 snapshots=2 physical_phase_claim=0\n");
		throw emu_fatalerror(0, "TMS320C54x isolated XC return barriers complete");
	}
	if (m_phase >= 523000 && m_phase < 523768)
	{
		unsigned const index = m_phase - 523000;
		auto const f = xc_latency_case(index);
		if (!m_rom4_codec_replays && (f.staged ? m_cpu->pipeline_cycles() == 5 + f.prefix : m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x600 + f.prefix))
		{
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save XC after its producer without replacing the retained Access condition");
			m_rom4_codec_replays = 1;
		}
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "XC producer and single predicate remain admitted");
		if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE)) { m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
		u64 const a = f.kind >= 2 && !(f.kind & 1) ? f.source() : f.take() ? 0x33 : 0;
		u64 const b = f.kind >= 2 && (f.kind & 1) ? f.source() : f.kind >= 2 && f.take() ? 0x33 : 0;
		if (m_cpu->state_int(tms320c54x_device::STATE_A) != a || m_cpu->state_int(tms320c54x_device::STATE_B) != b || m_cpu->state_int(tms320c54x_device::STATE_ST0) != f.status(true))
			osd_printf_info("XC condition divergence: case=%u staged=%u kind=%u gap=%u slots=%u initial=%u polarity=%u A=%04x expectedA=%04x B=%04x expectedB=%04x ST0=%04x expectedST0=%04x\n",
				index, f.staged, f.kind, f.gap, f.slots, f.initial, f.polarity, unsigned(m_cpu->state_int(tms320c54x_device::STATE_A)), unsigned(a),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_B)), unsigned(b), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST0)), f.status(true));
		expect(m_rom4_codec_replays == 1 && m_cpu->state_int(tms320c54x_device::STATE_A) == a && m_cpu->state_int(tms320c54x_device::STATE_B) == b &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.status(true) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800 &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x603 + f.prefix + f.gap + f.slots && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000,
			"XC retains old or settled TC/C/A/B once for all controlled words across one/two-word producers");
		if (!m_rom4_codec_replay)
		{
			expect(restore_snapshot(machine(), *m_check_timer, m_saved_pipeline) == STATERR_NONE, "Restore XC condition history and selected/annulled words");
			m_phase = 523000 + index; m_rom4_codec_replay = true;
			m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
		}
		if (index == 383) osd_printf_info("TMS320C54x staged XC conditions: PASS cases=384 restore=384 sources=TC,C,A,B producer_words=1,2 gaps=0,1,2,3 controlled_words=1,2 snapshots=2 physical_phase_claim=0\n");
		if (index < 767) { start_xc_latency(index + 1); return true; }
		osd_printf_info("TMS320C54x atomic XC conditions: PASS cases=384 restore=384 sources=TC,C,A,B producer_words=1,2 gaps=0,1,2,3 controlled_words=1,2 snapshots=2 physical_phase_claim=0\n");
		throw emu_fatalerror(0, "TMS320C54x isolated XC conditions complete");
	}
	if (m_phase >= 522000 && m_phase < 522128)
	{
		unsigned const index = m_phase - 522000, mode = index % 32, gap = mode % 4;
		bool const staged = index < 64, set = (mode / 4) & 1, initial = (mode / 8) & 1, tc = (mode / 16) & 1;
		if (!m_rom4_codec_replays && (staged ? m_cpu->pipeline_cycles() == 6 : m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x601))
		{
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save XC/status after publication with an older retained predicate");
			m_rom4_codec_replays = 1;
		}
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "XC and status producers remain admitted");
		if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE)) { m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
		bool const take = (gap < 2 ? initial : set) == tc;
		if (m_cpu->state_int(tms320c54x_device::STATE_A) != (take ? 0x33 : 0))
			osd_printf_info("XC/status divergence: staged=%u case=%u gap=%u initial_tc=%u producer_tc=%u predicate_tc=%u actual_A=%04x expected_A=%04x\n", staged, index, gap, initial, set, tc,
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_A)), take ? 0x33 : 0);
		expect(m_rom4_codec_replays == 1 && m_cpu->state_int(tms320c54x_device::STATE_A) == (take ? 0x33 : 0) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 0 && m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x605 + gap &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0xe05 | (set ? 0x1000 : 0)) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000,
			"XC tests its Access-stage status rather than either preceding Execute publication");
		if (!m_rom4_codec_replay)
		{
			expect(restore_snapshot(machine(), *m_check_timer, m_saved_pipeline) == STATERR_NONE, "Restore XC/status with the original retained predicate");
			m_phase = 522000 + index; m_rom4_codec_replay = true;
			m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
		}
		if (index == 63) osd_printf_info("TMS320C54x staged XC/status: PASS cases=64 restore=64 producers=SSBX,RSBX gaps=0,1,2,3 old_condition_words=2 snapshots=2 physical_phase_claim=0\n");
		if (index < 127) { start_xc_status(index + 1); return true; }
		osd_printf_info("TMS320C54x atomic XC/status: PASS cases=64 restore=64 producers=SSBX,RSBX gaps=0,1,2,3 old_condition_words=2 snapshots=2 physical_phase_claim=0\n");
		throw emu_fatalerror(0, "TMS320C54x isolated XC/status complete");
	}
	return false;
}

bool tms320c54x_test_state::check_conditional_register_results()
{
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 519000 && m_phase < 519144)
	{
		unsigned const index = m_phase - 519000;
		auto const f = conditional_register_case(index % 72);
		if (!m_rom4_codec_replays && m_cpu->state_int(tms320c54x_device::STATE_PC) == (f.take ? 0x700 : 0x601))
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save atomic return after its optional stack read and PC publication");
			m_rom4_codec_replays = 1;
		}
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "All twelve atomic signed accumulator return predicates are admitted");
		if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE)) { m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
		expect(m_rom4_codec_replays == 1 && m_pipeline_operand_reads == unsigned(f.take) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (f.take ? 0x22 : 0x11) && m_cpu->state_int(tms320c54x_device::STATE_B) == f.b &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 + unsigned(f.take) &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == (f.take ? 0x702 : 0x603) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == 0x1e05 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == 0x800,
			"Atomic conditional return tests the full signed accumulator and preserves status and exact stack ownership");
		if (!m_rom4_codec_replay)
		{
			expect(restore_snapshot(machine(), *m_check_timer, m_saved_pipeline) == STATERR_NONE, "Restore atomic conditional return without duplicating its stack read");
			m_pipeline_operand_reads = m_recurring_irq_snapshot[0];
			m_phase = 519000 + index; m_rom4_codec_replay = true;
			m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
		}
		if (index < 143) { start_atomic_conditional_return(index + 1); return true; }
		osd_printf_info("TMS320C54x atomic conditional returns: PASS cases=144 restore=144 predicates=12 patterns=6 snapshots=2 taken_reads=1 untaken_reads=0 stack_writes=0 silicon_phase_claim=0\n");
		throw emu_fatalerror(0, "TMS320C54x isolated atomic conditional returns complete");
	}
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
	if (m_phase >= 305000 && m_phase < 305064)
	{
		unsigned const index = m_phase - 305000, flags = (index >> 3) & 3, accumulator = (index >> 1) & 1;
		u16 const mask = 1U << (10 - accumulator);
		bool const take = BIT(flags, accumulator) == BIT(index, 2), delayed = index & 1;
		u16 const initial = 0x1805 | ((flags & 1) << 10) | ((flags & 2) << 8);
		if (!m_rom4_codec_replays && m_cpu->state_int(tms320c54x_device::STATE_PC) == (delayed || !take ? 0x602 : 0x700))
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_ST0) == (initial & ~mask), "Overflow branch consumes only its selected flag before any delay word");
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save consumed overflow predicate and optional delayed transfer");
			m_rom4_codec_replays = 1;
		}
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Atomic single-overflow branch is admitted");
		if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE))
		{
			m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
		}
		bool const slots = delayed || !take;
		expect(m_rom4_codec_replays == 1 && m_cpu->state_int(tms320c54x_device::STATE_A) == (take ? 0x22 : 0x11) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (slots ? 0x55 : 0) &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == (take ? 0x702 : 0x606) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == ((initial & ~mask) | (slots ? mask : 0)) &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000,
			"Overflow branch retains its original condition across flag-changing delay words");
		if (!m_rom4_codec_replay)
		{
			expect(restore_snapshot(machine(), *m_check_timer, m_saved_pipeline) == STATERR_NONE, "Restore overflow predicate or pending delayed transfer");
			m_phase = 305000 + index; m_rom4_codec_replay = true;
			m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
		}
		if (index < 63) { start_atomic_overflow_branch(index + 1); return true; }
		osd_printf_info("TMS320C54x atomic overflow branches: PASS cases=64 restore=64 forms=BC,BCD predicates=AOV,ANOV,BOV,BNOV flags=4 snapshots=2 selected_flag_clear=1 delay_condition_retained=1 silicon_phase_claim=0\n");
		throw emu_fatalerror(0, "TMS320C54x isolated overflow branches complete");
	}
	return false;
}


} // namespace c54x_test
