// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"
#include "tms320c54x_test_memory_immediate_schedule.h"
#include "tms320c54x_test_rmw_schedule.h"
#include "tms320c54x_test_shifted_schedule.h"
#include "tms320c54x_test_register_schedule.h"

namespace c54x_test {















std::array<u16, 2> tms320c54x_test_state::addm_inputs(unsigned variant)
{
	return std::array<std::array<u16, 2>, 8>{{{0, 2}, {0x7fff, 1}, {0x8007, 0xfff8}, {0xffff, 1},
		{0x8000, 0x8000}, {0x7fff, 0x7fff}, {0xffff, 0xffff}, {0x1234, 0xffff}}}[(variant >> 3) & 7];
}

std::array<u16, 2> tms320c54x_test_state::addm_expected(unsigned variant)
{
	auto const input = addm_inputs(variant);
	s32 const left = BIT(variant, 0) ? s16(input[0]) : s32(input[0]);
	s32 const right = BIT(variant, 0) ? s16(input[1]) : s32(input[1]);
	s32 const sum = left + right; bool const overflow = sum < -32768 || sum > 32767;
	u16 const word = overflow && BIT(variant, 1) ? u16(sum < -32768 ? 0x8000 : 0x7fff) : u16(sum);
	u16 const flags = u16((0xaa08 | (BIT(variant, 2) << 10)) & ~0x800) |
		(u64(u32(left)) + u32(right) > 0xffffffffULL ? 0x800 : 0) | (overflow ? 0x400 : 0);
	return {word, flags};
}














tms320c54x_test_state::long_pair_fixture_data tms320c54x_test_state::long_pair_fixture(unsigned variant)
{
	if (variant >= 6912) return long_add_fixture(variant - 6912);
	unsigned const form = variant % 9, destination = (variant / 9) % 2, pattern = (variant / 18) % 8;
	bool const dual = (variant / 144) % 2, sxm = (variant / 288) % 2, ovm = (variant / 576) % 2, odd = (variant / 1152) % 2;
	unsigned const reg = odd ? 6 : 1;
	u16 const initial_ar = (form == 7 ? 0x1007 : 0x1000) + odd;
	s16 const offset = odd ? -2 : 2;
	u16 const address = form == 0 ? 0x402 + odd : form == 1 ? 0x1002 + odd : form == 4 ? initial_ar + 2 :
		form >= 5 && form <= 7 ? u16(initial_ar + offset) : form == 8 ? 0x1400 + odd : initial_ar;
	u16 const next_ar = form == 2 || form == 4 ? initial_ar + 2 : form == 3 ? initial_ar - 2 :
		form == 6 || form == 7 ? address : initial_ar;
	u8 const mode = form < 2 ? 2 + odd : form == 8 ? 0xf8 : std::array<u8, 6>{0x90, 0x88, 0x98, 0xe0, 0xe8, 0xf0}[form - 2] | reg;
	u64 const source = std::array<u64, 8>{0, 0x10000, 0x7fffffff, 0xff80000000ULL, 0x0180000000ULL, 0xff7fffffffULL, 0x56783933, 0x80aa001234ULL}[pattern];
	u32 const memory = std::array<u32, 8>{1, 1, 0xffffffffU, 1, 0, 0x80000000U, 0x15343456, 0xdeadbeefU}[pattern];
	u16 const st0 = 0x1808 | (pattern & 1 ? 0x600 : 0);
	u16 const st1 = 0x800 | (dual << 7) | (sxm << 8) | (ovm << 9) | (form == 1 ? 0x4000 : 0);
	u64 result;
	u16 result_st0 = st0;
	if (dual)
	{
		s64 const high_left = sxm ? s16(source >> 16) : u16(source >> 16), high_right = sxm ? s16(memory >> 16) : u16(memory >> 16);
		s64 const high_result = high_left - high_right;
		result = (u64(high_result) & 0xffffff) * 65536 + u16(u16(source) - u16(memory));
		result_st0 = (st0 & ~0x800) | (u16(source >> 16) >= u16(memory >> 16) ? 0x800 : 0);
	}
	else
	{
		s64 const left = BIT(source, 39) ? s64(source) - (s64(1) << 40) : s64(source);
		s64 const right = sxm && BIT(memory, 31) ? s64(memory) - (s64(1) << 32) : s64(memory);
		s64 const difference = left - right;
		bool const overflow = difference < -2147483648LL || difference > 2147483647LL;
		result = u64(overflow && ovm ? (difference < 0 ? -2147483648LL : 2147483647LL) : difference) & 0xffffffffffULL;
		result_st0 = (st0 & ~0x800) | (u32(source) >= memory ? 0x800 : 0) | (overflow ? (destination ? 0x200 : 0x400) : 0);
	}
	return {form, reg, form >= 5 ? 6U : 5U, address, next_ar, form == 8 ? address : u16(offset),
		u16(0x5400 | (destination << 8) | mode), st0, st1, source, result, memory, result_st0};
}

tms320c54x_test_state::long_pair_fixture_data tms320c54x_test_state::long_add_fixture(unsigned variant)
{
	unsigned const v = variant % 4608, form = v % 9, selector = (v / 9) % 4, pattern = (v / 36) % 8, flags = (v / 288) % 8;
	bool const dual = flags & 1, sxm = flags & 2, ovm = flags & 4, odd = (v / 2304) % 2;
	auto f = long_pair_fixture(form + 9 * (selector & 1) + 18 * pattern + 144 * dual + 288 * sxm + 576 * ovm + 1152 * odd);
	f.opcode = 0x5000 | ((selector >> 1) << 9) | ((selector & 1) << 8) | (f.opcode & 0xff);
	if (form < 2) { f.opcode += 2; f.address += 2; }
	if (dual)
	{
		s64 const left_high = sxm ? s16(f.source >> 16) : u16(f.source >> 16), right_high = sxm ? s16(f.memory >> 16) : u16(f.memory >> 16);
		f.result = ((u64(left_high + right_high) & 0xffffff) << 16) | u16(u16(f.source) + u16(f.memory));
		f.result_st0 = (f.st0 & ~0x800) | (u32(u16(f.source >> 16)) + u16(f.memory >> 16) > 0xffff ? 0x800 : 0);
	}
	else
	{
		s64 const left = BIT(f.source, 39) ? s64(f.source) - (s64(1) << 40) : s64(f.source);
		s64 const right = sxm && BIT(f.memory, 31) ? s64(f.memory) - (s64(1) << 32) : s64(f.memory);
		s64 const sum = left + right;
		bool const overflow = sum < -2147483648LL || sum > 2147483647LL;
		f.result = u64(overflow && ovm ? (sum < 0 ? -2147483648LL : 2147483647LL) : sum) & 0xffffffffffULL;
		f.result_st0 = (f.st0 & ~0x800) | (u64(u32(f.source)) + f.memory > 0xffffffffULL ? 0x800 : 0) | (overflow ? (selector & 1 ? 0x200 : 0x400) : 0);
	}
	return f;
}

tms320c54x_test_state::long_pair_fixture_data tms320c54x_test_state::long_reverse_fixture(unsigned variant)
{
	auto f = long_pair_fixture(variant);
	f.opcode = (f.opcode & 0x01ff) | 0x5800;
	bool const dual = f.st1 & 0x80, sxm = f.st1 & 0x100, ovm = f.st1 & 0x200;
	if (dual)
	{
		s64 const memory_high = sxm ? s16(f.memory >> 16) : u16(f.memory >> 16);
		s64 const source_high = sxm ? s16(f.source >> 16) : u16(f.source >> 16);
		f.result = ((u64(memory_high - source_high) & 0xffffff) << 16) | u16(u16(f.memory) - u16(f.source));
		f.result_st0 = (f.st0 & ~0x800) | (u16(f.memory >> 16) >= u16(f.source >> 16) ? 0x800 : 0);
	}
	else
	{
		s64 const memory = sxm && (f.memory & 0x80000000U) ? s64(f.memory) - (s64(1) << 32) : s64(f.memory);
		s64 const source = f.source & 0x8000000000ULL ? s64(f.source) - (s64(1) << 40) : s64(f.source);
		s64 const difference = memory - source;
		bool const overflow = difference < -2147483648LL || difference > 2147483647LL;
		f.result = u64(overflow && ovm ? (difference < 0 ? -2147483648LL : 2147483647LL) : difference) & 0xffffffffffULL;
		f.result_st0 = (f.st0 & ~0x800) | (f.memory >= u32(f.source) ? 0x800 : 0) | (overflow ? BIT(f.opcode, 8) ? 0x200 : 0x400 : 0);
	}
	return f;
}

void tms320c54x_test_state::start_pipeline_long_pair(unsigned variant)
{
	if (variant % 9 == 4) ++variant; // Short MOD=3 is write-only; preserve raw IDs for the other forms.
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = profile_is("drsub") ? long_reverse_fixture(variant) : long_pair_fixture(variant);
	bool const source = (f.opcode & 0xfc00) == 0x5000 ? BIT(f.opcode, 9) : BIT(f.opcode, 8);
	u32 const base = f.reg == 6 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) program.write_word(base + 0x600 + i, i ? 0xf495 : f.opcode);
	if (f.form >= 5) program.write_word(base + 0x601, f.literal);
	set_registers(*m_cpu, {
		{cpu_regs::STATE_IDLE, 0},
		{cpu_regs::STATE_TCR, 0x10}, {cpu_regs::STATE_PMST, 0}, {cpu_regs::STATE_ILLEGAL, 0},
		{cpu_regs::STATE_ST0, f.st0}, {cpu_regs::STATE_ST1, f.st1}, {cpu_regs::STATE_SP, 0x1000},
		{cpu_regs::STATE_AR0 + f.reg, (f.form == 7 ? 0x1007 : 0x1000) + (f.reg == 6)}, {cpu_regs::STATE_BK, 16},
		{cpu_regs::STATE_A, source ? 0x246813579aULL : f.source},
		{cpu_regs::STATE_B, source ? f.source : 0x246813579aULL}, {cpu_regs::STATE_IFR, 0}, {cpu_regs::STATE_IMR, 0},
	});
	m_cpu->space(AS_DATA).write_word(f.address, u16(f.memory >> 16));
	m_cpu->space(AS_DATA).write_word(f.address ^ 1, u16(f.memory));
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "dsub_long_pair",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(m_pipeline_operand_reads < 2 && at == (f.address ^ m_pipeline_operand_reads) &&
				word == (m_pipeline_operand_reads ? u16(f.memory) : u16(f.memory >> 16)) && m_cpu->pipeline_cycles() == f.read_cycle,
				"Long arithmetic samples its retained MSW/LSW pair once in parity-defined order"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "dsub_no_memory_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Long arithmetic does not write data memory"); });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + 0x601, "dsub_offset",
		[this, f](offs_t, u16 &word, u16) { if (machine().side_effects_disabled() || f.form < 5) return;
			expect(word == f.literal && !m_pipeline_extensions_reads, "Long arithmetic retains its extension once"); ++m_pipeline_extensions_reads; });
	m_phase = 252630; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::accumulator_store_fixture tms320c54x_test_state::long_accumulator_store_fixture(unsigned variant)
{
	unsigned const form = variant % 25, kind = form == 24 ? 3 : form / 8, reg = form == 24 ? 0 : form % 8;
	unsigned const selector = (variant / 100) % 4, flags = (variant / 400) % 4;
	bool const compatibility = (variant / 6400) % 2;
	s16 const offset = std::array<s16, 4>{0, 1, -1, s16(0xf495)}[(variant / 25) % 4];
	u64 const source = std::array<u64, 4>{0x12345678, 0x0180000000ULL, 0xff7fffffffULL, 0xffffffffffULL}[(variant / 1600) % 4];
	u16 const initial = kind == 2 ? 0x4007 : 0x1007, bk = kind == 2 ? 8192 : 16;
	u16 const address = kind == 3 ? u16(0x1500 + offset) : kind == 2 ? circular_fixture_next(initial, offset, bk) : u16(initial + offset);
	u32 result = u32(source);
	if (flags & 1)
	{
		if (!(flags & 2)) { if (source > 0xffffffffULL) result = 0xffffffff; }
		else
		{
			s64 const signed_source = source & (1ULL << 39) ? s64(source) - (1LL << 40) : s64(source);
			if (signed_source < -0x80000000LL) result = 0x80000000;
			else if (signed_source > 0x7fffffffLL) result = 0x7fffffff;
		}
	}
	return {reg, compatibility && !reg ? 3U : reg, kind, selector,
		u16(0x8000 | (selector << 8) | (kind == 3 ? 0xf8 : 0xe0 | (kind << 3) | reg)),
		u16(kind == 3 ? address : u16(offset)), address, u16(kind && kind != 3 ? address : initial),
		u16(compatibility ? 0x6608 : 0x608), u16(0x800 | ((flags & 2) ? 0x100 : 0) | (compatibility ? 0x20 : 0)),
		u16(flags & 1), u16(result >> ((selector & 2) ? 16 : 0)), source, initial, bk};
}



std::array<u16, 2> tms320c54x_test_state::long_rmw_expected(unsigned arithmetic, unsigned operation, u16 flags)
{
	auto const input = addm_inputs(arithmetic);
	if (operation) return {u16(operation == 1 ? input[0] & input[1] : operation == 2 ? input[0] | input[1] : input[0] ^ input[1]), flags};
	auto const sum = addm_expected(arithmetic);
	return {sum[0], u16((flags & ~0x0c00) | (sum[1] & 0x0c00))};
}

tms320c54x_test_state::accumulator_store_fixture tms320c54x_test_state::absolute_xorm_fixture(unsigned variant)
{
	auto f = long_accumulator_store_fixture(24 + 25 * (variant % 4) + (((variant / 256) % 2) ? 6400 : 0));
	f.reg = (variant / 512) % 2 ? 6 : 0;
	return f;
}

tms320c54x_test_state::memory_arithmetic_fixture tms320c54x_test_state::memory_arithmetic_case(unsigned variant, bool unsigned_memory, unsigned shift16_selector)
{
	unsigned const form = variant % 18, reg = (variant / 18) % 8, selector = (variant / 144) % 4, pattern = (variant / 576) % 8, flags = (variant / 4608) % 4;
	bool const shift16 = shift16_selector < 8;
	bool const compatibility = (variant / 18432) % 2, subtract = shift16 ? bool(shift16_selector & 4) : bool(selector & 2), sxm = flags & 1, ovm = flags & 2;
	unsigned const selected = compatibility && !reg ? 3 : reg;
	u64 const source = std::array<u64, 8>{0, 0xffffffff, 0x7fffffff, 0xff80000000ULL, 0x017fffffffULL, 0xff7fffffffULL, 0x56783933, 0x80aa001234ULL}[pattern];
	u16 const memory = std::array<u16, 8>{1, 1, 1, 0xffff, 0x8000, 0xffff, 0x1234, 0xffff}[pattern];
	s16 const offset = std::array<s16, 8>{0, 1, -1, 17, s16(0xf495), 0x7fff, s16(0x8000), 2}[pattern];
	u8 const mode = form < 2 ? reg * 16 + pattern + (sxm ? 8 : 0) : form == 17 ? 0xf8 : 0x80 | ((form - 2) << 3) | reg;
	// AR0 starts at 0x5007, including when it is the selected pointer.
	// Indexed and long circular forms need a buffer large enough for that
	// step and every signed literal (SPRU131G section 5.5.3.4).
	u16 const bk = form == 11 || form == 13 || form == 16 ? 0x8000 : 16;
	u16 const address = form == 0 ? 0x200 + mode : form == 1 ? 0x1000 + mode : form == 5 ? 0x5008 :
		form == 16 ? circular_fixture_next(0x5007, offset, bk) : form >= 14 && form < 17 ? u16(0x5007 + offset) :
		form == 17 ? u16(0x1500 + offset) : 0x5007;
	u16 const next_ar = form < 2 || form == 17 ? 0x5007 : form >= 14 ? (form == 14 ? 0x5007 : address) :
		form == 11 || form == 13 ? circular_fixture_next(0x5007, form == 11 ? -0x5007 : 0x5007, bk) :
		std::array<u16, 12>{0x5007, 0x5006, 0x5008, 0x5008, 0, 0, 0xa00e, 0x2803, 0x5006, 0x5000, 0x5008, 0x500e}[form - 2];
	u16 const st0 = (shift16 ? 0x1004 | (pattern & 1 ? 0x800 : 0) : 0x1804) | (compatibility ? 0x6000 : 0) | (pattern & 1 ? 0x600 : 0);
	u16 const st1 = 0x800 | (sxm ? 0x100 : 0) | (ovm ? 0x200 : 0) | (compatibility ? 0x20 : 0) | (form == 1 ? 0x4000 : 0);
	u16 result_st0 = compatibility && reg && form >= 2 && form != 17 ? (st0 & 0x1fff) | (reg << 13) : st0;
	s64 const left = source & (1ULL << 39) ? s64(source) - (1LL << 40) : s64(source);
	s64 const right = (sxm && !unsigned_memory ? s64(s16(memory)) : s64(memory)) * (shift16 ? 65536 : 1), sum = subtract ? left - right : left + right;
	bool const overflow = sum < -2147483648LL || sum > 2147483647LL;
	bool const carry = subtract ? u32(source) >= u32(right) : u64(u32(source)) + u32(right) > 0xffffffffULL;
	if (!shift16 || carry != subtract) result_st0 = (result_st0 & ~0x800) | (carry ? 0x800 : 0);
	result_st0 |= overflow ? ((shift16 ? shift16_selector : selector) & 1 ? 0x200 : 0x400) : 0;
	u64 const result = u64(overflow && ovm ? (sum < 0 ? -2147483648LL : 2147483647LL) : sum) & 0xffffffffffULL;
	return {form, reg, selected, form >= 14 ? 6U : 5U, u16((shift16 ? (subtract ? 0x4000 : 0x3c00) | ((shift16_selector & 3) << 8) : (unsigned_memory ? (subtract ? 0x0a00 : 0x0200) : subtract ? 0x0800 : 0) | ((selector & 1) << 8)) | mode),
		u16(form == 17 ? address : u16(offset)), address, next_ar, memory, st0, st1, result_st0, source, result, bk};
}












tms320c54x_test_state::memory_arithmetic_fixture tms320c54x_test_state::shifted_memory_load_case(unsigned variant)
{
	auto f = unsigned_load_case(variant);
	f.opcode = (f.opcode & 0x01ff) | 0x4400;
	s64 const value = BIT(f.st1, 8) ? s16(f.memory) : f.memory;
	f.result = u64(value * 0x10000) & 0xffffffffffULL;
	return f;
}

tms320c54x_test_state::immediate_arithmetic_fixture tms320c54x_test_state::immediate_arithmetic_case(unsigned variant)
{
	bool const producer = variant >= 27648;
	unsigned const v = producer ? (variant - 27648) % 48 : variant % 9216;
	bool const add = !producer && v >= 8704;
	unsigned const u = add ? v - 8704 : v;
	unsigned const shift = producer ? 0 : add ? 16 : u % 17;
	unsigned const selector = producer || add ? u % 4 : (u / 17) % 4;
	unsigned const pattern = producer ? (u / 4) % 4 : add ? (u / 4) % 8 : (u / 68) % 8;
	unsigned const flags = producer ? 1 : add ? (u / 32) % 8 : (u / 544) % 8;
	unsigned const gap = producer ? (u / 16) % 3 : 0;
	u16 const literal = producer ? 1 : std::array<u16, 8>{1, 1, 0xffff, 1, 0x8000, 0xffff, 0xf495, 0xffff}[pattern];
	u64 const source = producer ? u64(s64(s16(std::array<u16, 4>{0, 1, 0x8000, 0xffff}[pattern]))) & 0xffffffffffULL :
		std::array<u64, 8>{0, 0xffffffff, 0x7fffffff, 0xff80000000ULL, 0x017fffffffULL, 0xff7fffffffULL, 0x56783933, 0x80aa001234ULL}[pattern];
	u16 const st0 = 0x1804 | (flags & 4 ? 0x600 : 0), st1 = 0x800 | (flags & 1 ? 0x100 : 0) | (flags & 2 ? 0x200 : 0);
	s64 const left = BIT(source, 39) ? s64(source) - (s64(1) << 40) : s64(source);
	s64 const right = (flags & 1 ? s64(s16(literal)) : s64(literal)) * (s64(1) << shift);
	s64 const sum = add ? left + right : left - right;
	bool const overflow = sum < -2147483648LL || sum > 2147483647LL;
	bool const carry = add ? u64(u32(source)) + u32(right) > 0xffffffffULL : u32(source) >= u32(right);
	u16 const result_st0 = (st0 & ~0x800) | (carry ? 0x800 : 0) | (overflow ? (selector & 1 ? 0x200 : 0x400) : 0);
	u64 const result = u64((flags & 2) && overflow ? (sum < 0 ? -2147483648LL : 2147483647LL) : sum) & 0xffffffffffULL;
	u16 const opcode = (shift == 16 ? 0xf060 | !add : add ? 0xf000 : 0xf010) | ((selector >> 1) << 9) | ((selector & 1) << 8) | (shift == 16 ? 0 : shift);
	u32 const base = (producer ? selector & 1 : add ? (u / 256) % 2 : (u / 4352) % 2) ? 0x20000 : 0;
	return {selector, shift, flags, gap, producer ? 9 + gap : 7, producer, add, opcode, literal, st0, st1, result_st0, source, result, base};
}

void tms320c54x_test_state::start_pipeline_immediate_arithmetic(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = immediate_arithmetic_case(variant);
	u32 const sub_pc = f.base + 0x600 + (f.producer ? 2 + f.gap : 0);
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	if (f.producer) { program.write_word(f.base + 0x600, 0xf020 | ((f.selector >> 1) << 8)); program.write_word(f.base + 0x601, f.source); }
	program.write_word(sub_pc, f.opcode); program.write_word(sub_pc + 1, f.literal);
	if (f.producer) program.write_word(sub_pc + 2, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.producer || (f.selector & 2) ? 0x1111222233ULL : f.source);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, f.producer || !(f.selector & 2) ? 0x4444555566ULL : f.source);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "immediate_arithmetic_no_read",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Immediate arithmetic does not read data memory"); });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "immediate_arithmetic_no_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Immediate arithmetic modifies only accumulator/status"); });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, sub_pc + 1, "immediate_arithmetic_literal",
		[this, f, sub_pc](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled() || (at != f.base + 0x601 && at != sub_pc + 1)) return;
			bool const producer_literal = f.producer && !m_pipeline_extensions_reads;
			expect(at == (producer_literal ? f.base + 0x601 : sub_pc + 1) && word == (producer_literal ? u16(f.source) : f.literal) &&
				m_pipeline_extensions_reads < (f.producer ? 2U : 1U), "Immediate arithmetic retains each literal once, without executing opcode-shaped data"); ++m_pipeline_extensions_reads; });
	m_phase = 253110; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::register_arithmetic_fixture tms320c54x_test_state::register_arithmetic_case_full(unsigned variant)
{
	bool const producer = variant >= 196608;
	unsigned const v = producer ? (variant - 196608) % 96 : variant % 65536;
	unsigned const operation = v % 2, selector = producer ? (v / 2) % 4 : (v / 64) % 4;
	unsigned const pattern = producer ? (v / 8) % 4 : (v / 256) % 8;
	unsigned const flags = producer ? 1 : (v / 2048) % 8, gap = producer ? (v / 32) % 3 : 0;
	bool const asm_shift = !producer && (v / 16384) % 2;
	int const shift = producer ? 0 : int((v / 2) % 32) - 16;
	std::array<u64, 2> initial = producer ? std::array<u64, 2>{0x10, 0x20} : std::array<u64, 2>{
		std::array<u64, 8>{0, 1, 0x7fffffff, 0xff80000000ULL, 0x017fffffffULL, 0xff7fffffffULL, 0x80aa001234ULL, 0xffffffff55ULL}[pattern],
		std::array<u64, 8>{1, 0xffffffff, 0xff80000000ULL, 0x7fffffff, 0xff7fffffffULL, 0x017fffffffULL, 0x123456789aULL, 0x80000000}[pattern]};
	auto before = initial;
	if (producer) before[selector >> 1] = u64(s64(s16(std::array<u16, 4>{0, 1, 0x8000, 0xffff}[pattern]))) & 0xffffffffffULL;
	u64 const source = before[selector >> 1];
	u64 operand = 0;
	for (int bit = 0; bit < 40; ++bit)
	{
		int const source_bit = bit - shift;
		if ((source_bit >= 0 && source_bit < 40 && BIT(source, source_bit)) ||
			(shift < 0 && source_bit >= 40 && (flags & 1) && BIT(source, 39))) operand |= u64(1) << bit;
	}
	u64 const destination = before[selector & 1];
	s64 const left = BIT(destination, 39) ? s64(destination) - (s64(1) << 40) : s64(destination);
	s64 const right = BIT(operand, 39) ? s64(operand) - (s64(1) << 40) : s64(operand);
	s64 const sum = operation ? left - right : left + right;
	bool const overflow = sum < -2147483648LL || sum > 2147483647LL;
	bool const carry = operation ? u32(destination) >= u32(operand) : u64(u32(destination)) + u32(operand) > 0xffffffffULL;
	u16 const st0 = 0x1804 | (flags & 4 ? 0x600 : 0);
	u16 const st1 = 0x800 | (flags & 1 ? 0x100 : 0) | (flags & 2 ? 0x200 : 0) | (asm_shift ? shift & 31 : 0x13);
	u16 const result_st0 = (st0 & ~0x800) | (carry ? 0x800 : 0) | (overflow ? (selector & 1 ? 0x200 : 0x400) : 0);
	auto after = before;
	after[selector & 1] = u64((flags & 2) && overflow ? (sum < 0 ? -2147483648LL : 2147483647LL) : sum) & 0xffffffffffULL;
	u16 const opcode = (asm_shift ? 0xf480 | operation : 0xf400 | (operation << 5) | (shift & 31)) | ((selector >> 1) << 9) | ((selector & 1) << 8);
	u32 const base = (producer ? selector & 1 : (v / 32768) % 2) ? 0x20000 : 0;
	return {selector, gap, producer ? 8 + gap : 6, producer, opcode, st0, st1, result_st0, initial, before, after, base};
}

void tms320c54x_test_state::start_pipeline_accumulator_arithmetic(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	if (!variant) { m_register_bank_coverage = {}; m_register_bank_replays = 0; }
	auto const f = variant >= 196896 ? register_load_case(variant - 196896) : register_arithmetic_case_full(variant);
	u32 const opcode_pc = f.base + 0x600 + (f.producer ? 2 + f.gap : 0);
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 32; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	if (f.producer) { program.write_word(f.base + 0x600, 0xf020 | ((f.selector >> 1) << 8)); program.write_word(f.base + 0x601, f.before[f.selector >> 1]); }
	program.write_word(opcode_pc, f.opcode); if (f.producer) program.write_word(opcode_pc + 1, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.initial[0]); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.initial[1]);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_extensions_reads = m_pipeline_operand_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "register_alu_no_read",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Register ALU does not read data memory"); });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "register_alu_no_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Register ALU publishes only accumulator/status"); });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x600, opcode_pc, "register_alu_fetch",
		[this, f, opcode_pc](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			if (at == opcode_pc) { expect(word == f.opcode && !m_pipeline_extensions_reads, "Register ALU fetches its one-word opcode once"); ++m_pipeline_extensions_reads; }
			else if (f.producer && at == f.base + 0x601) { expect(word == u16(f.before[f.selector >> 1]) && !m_pipeline_operand_reads, "The preceding load retains its one literal"); ++m_pipeline_operand_reads; } });
	m_phase = 253130; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::memory_multiply_fixture tms320c54x_test_state::memory_multiply_case(unsigned variant, bool square)
{
	if (square)
	{
		auto f = memory_multiply_case(variant + 55296);
		f.operand.opcode = 0x2600 | (f.operand.opcode & 0x1ff);
		s64 const value = s16(f.operand.memory);
		s64 const product = value * value * (BIT(f.operand.st1, 6) ? 2 : 1);
		bool const overflow = product > 2147483647LL;
		f.result = u64(overflow && BIT(f.operand.st1, 9) ? 2147483647LL : product);
		f.result_st0 = f.operand.st0 | (overflow ? (BIT(f.operand.opcode, 8) ? 0x200 : 0x400) : 0);
		return f;
	}
	if (variant >= 165888)
	{
		auto f = memory_multiply_case(variant - 165888);
		f.operand.opcode = 0x3000 | u8(f.operand.opcode);
		f.result_st0 = f.operand.st0;
		return f;
	}
	if (variant >= 110592)
	{
		unsigned const v = variant - 110592;
		auto f = memory_multiply_case(v + 55296);
		unsigned const operation = v < 30720 ? (v % 10240 / 10) % 4 : ((v - 30720) % 8192 / 16) % 4;
		f.operand.opcode = (operation & 2 ? 0x2200 : 0x2000) | (f.operand.opcode & 0x1ff);
		s64 product = s64(s16(f.t)) * s16(f.operand.memory) * (BIT(f.operand.st1, 6) ? 2 : 1);
		if (operation & 2) product = (product + 32768) & ~s64(65535);
		bool const overflow = product < -2147483648LL || product > 2147483647LL;
		f.result = u64(overflow && BIT(f.operand.st1, 9) ? product < 0 ? -2147483648LL : 2147483647LL : product) & 0xffffffffffULL;
		f.result_st0 = f.operand.st0 | (overflow ? (BIT(f.operand.opcode, 8) ? 0x200 : 0x400) : 0);
		return f;
	}
	if (variant >= 55296)
	{
		unsigned const v = variant - 55296;
		auto f = memory_multiply_case(v);
		unsigned const operation = v < 30720 ? (v % 10240 / 10) % 4 : ((v - 30720) % 8192 / 16) % 4;
		f.operand.opcode = 0x2400 | ((operation & 1) << 8) | u8(f.operand.opcode);
		f.t = operation & 2 ? 0xffff : 0x8000;
		if (f.operand.address == 0x0e) f.operand.memory = f.t;
		s64 const product = s64(f.t) * f.operand.memory * (BIT(f.operand.st1, 6) ? 2 : 1);
		bool const overflow = product > 2147483647LL;
		f.result = u64(overflow && BIT(f.operand.st1, 9) ? 2147483647LL : product);
		f.result_st0 = f.operand.st0 | (overflow ? (BIT(f.operand.opcode, 8) ? 0x200 : 0x400) : 0);
		return f;
	}
	bool const mapped = variant >= 30720;
	unsigned const v = mapped ? (variant - 30720) % 8192 : variant % 10240;
	unsigned const operation = mapped ? (v / 16) % 4 : (v / 10) % 4;
	unsigned const pattern = mapped ? (v / 64) % 8 : (v / 40) % 8, flags = mapped ? (v / 512) % 16 : (v / 320) % 16;
	unsigned const kind = v % 10, reg = (v / 5120) % 2 ? 6 : 1;
	auto f = memory_arithmetic_case(std::array<unsigned, 10>{0, 1, 2, 3, 4, 5, 14, 15, 16, 17}[kind] + 18 * reg + 576 * pattern);
	s32 const high = std::array<s32, 8>{0, 1, -1, 32767, -32768, 65535, -65536, 0x1234}[pattern];
	u64 const a = (pattern & 1 ? 0xfe00000000ULL : 0) | ((u64(high) & 0x1ffff) << 16) | (0x1234 + pattern);
	u64 const b = std::array<u64, 8>{0, 1, 0x7fffffff, 0xff80000000ULL, 0x017fffffffULL, 0xff7fffffffULL, 0x1234567833ULL, 0x80aa001234ULL}[pattern];
	if (mapped)
	{
		f.form = 17; f.reg = f.selected = 1; f.read_cycle = 6; f.address = 0x08 + v % 16; f.literal = f.address; f.next_ar = 0x5007;
		f.opcode = (0x3100 + operation * 0x200) | 0xf8;
		f.memory = f.address <= 0x0a ? u16(a >> ((f.address - 8) * 16)) : f.address <= 0x0d ? u16(b >> ((f.address - 0x0b) * 16)) :
			f.address == 0x0e ? 0x9abc : f.address == 0x0f ? 0x1357 : 0x5007;
	}
	else
	{
		f.opcode = (0x3100 + operation * 0x200) | u8(f.opcode);
		if (f.form < 2) { f.opcode = (f.opcode & 0xff00) | (0x0b + (reg == 6)); f.address = (f.form ? 0x1000 : 0x200) + u8(f.opcode); }
		f.memory = std::array<u16, 8>{0, 1, 0xffff, 0x7fff, 0x8000, 0x8000, 0xffff, 0x5678}[pattern];
	}
	f.st0 = 0x1004 | (pattern & 1 ? 0x600 : 0) | (pattern & 2 ? 0x800 : 0);
	f.st1 = 0x800 | (flags & 1 ? 0x40 : 0) | (flags & 2 ? 0x200 : 0) | (flags & 8 ? 0x100 : 0) | (f.form == 1 ? 0x4000 : 0);
	s64 product = s64(high) * s16(f.memory) * (flags & 1 ? 2 : 1);
	if (operation && (flags & 7) == 7) product = std::clamp(product, s64(-2147483648LL), s64(2147483647LL));
	s64 const signed_b = BIT(b, 39) ? s64(b) - (s64(1) << 40) : s64(b);
	s64 result = !operation ? product : operation == 1 ? signed_b - product : signed_b + product;
	if (operation == 3) result = (result + 32768) & ~s64(65535);
	bool const overflow = result < -2147483648LL || result > 2147483647LL;
	u64 const stored = u64(overflow && (flags & 2) ? (result < 0 ? -2147483648LL : 2147483647LL) : result) & 0xffffffffffULL;
	return {f, a, b, stored, u16(f.st0 | (overflow ? 0x200 : 0)), u16(flags & 4 ? 2 : 0)};
}

void tms320c54x_test_state::start_pipeline_memory_multiply(unsigned variant)
{
	unsigned const raw = variant % 55296;
	if (raw < 30720 && raw % 10 == 5) ++variant; // Omit write-only short MOD=3; keep mapped cases and raw IDs.
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = memory_multiply_case(variant, profile_is("squr-smem")); auto const &o = f.operand;
	u32 const base = o.reg == 6 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, o.opcode); if (o.form >= 14) program.write_word(base + 0x601, o.literal);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, f.pmst);
	m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, o.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, o.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_BK, o.bk);
	for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, 0x5007);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, f.t); m_cpu->set_state_int(tms320c54x_device::STATE_TRN, 0x1357);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	if (o.address >= 0x60) m_cpu->space(AS_DATA).write_word(o.address, o.memory);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "memory_multiply_source",
		[this, o](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == o.address && word == o.memory && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == o.read_cycle, "Memory multiply retains one Read sample"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "memory_multiply_no_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Memory multiply does not write data memory"); });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + 0x601, "memory_multiply_literal",
		[this, o](offs_t, u16 &word, u16) { if (machine().side_effects_disabled() || o.form < 14) return;
			expect(word == o.literal && !m_pipeline_extensions_reads, "Memory multiply retains its offset/address once"); ++m_pipeline_extensions_reads; });
	m_phase = 253150; m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}



tms320c54x_test_state::shifted_arithmetic_fixture tms320c54x_test_state::shifted_arithmetic_case(unsigned variant, bool bk)
{
	if (bk)
	{
		auto f = shifted_arithmetic_case(147456 + (variant % 4096) * 8 + ((variant / 4096) % 3) * 32768);
		f.operand.address = f.operand.literal = 0x19;
		f.operand.reg = variant / 12288 ? 6 : 1;
		return f;
	}
	if (variant >= 245760)
	{
		unsigned const n = variant - 245760; bool const mapped = n >= 18432;
		unsigned const v = mapped ? (n - 18432) % 4096 : n % 6144;
		unsigned const destination = mapped ? (v / 8) % 2 : (v / 12) % 2;
		unsigned const pattern = mapped ? (v / 16) % 8 : (v / 24) % 8;
		unsigned const memory_pattern = mapped ? (v / 128) % 8 : (v / 192) % 8;
		unsigned const flags = mapped ? (v / 1024) % 4 : (v / 1536) % 4;
		auto o = memory_arithmetic_case(std::array<unsigned, 6>{0, 1, 4, 14, 15, 17}[v % 6] + 18 * ((v / 6) % 2 ? 6 : 1) + 576 * pattern + 4608 * flags);
		o.opcode = 0x1e00 | destination << 8 | u8(o.opcode);
		o.memory = std::array<u16, 8>{0, 1, 0x7fff, 0x8000, 0xffff, 7, 2, 0x1234}[memory_pattern];
		if (mapped) { o.form = 17; o.reg = o.selected = 1; o.address = 0x10 + v % 8; o.literal = o.address; o.opcode = 0x1ef8 | destination << 8; o.read_cycle = 6; o.st1 &= ~0x4000; }
		u64 const source = std::array<u64, 8>{0, 1, 0x7fffffff, 0xff80000000ULL, 0x017fffffffULL, 0xff7fffffffULL, 0x1fffffff, 0xffffffffULL}[pattern];
		s64 const dividend = BIT(source, 39) ? s64(source) - (s64(1) << 40) : s64(source);
		s64 const divisor = flags & 1 ? s16(o.memory) : s64(o.memory);
		s64 const difference = dividend - divisor * 32768;
		bool const subtract = difference >= 0, overflow = difference < -2147483648LL || difference > 2147483647LL;
		u16 const st0 = (o.st0 & ~0x800) | (subtract ? 0x800 : 0) | (overflow ? destination ? 0x200 : 0x400 : 0);
		u64 const result = u64(subtract ? difference * 2 + 1 : dividend * 2) & 0xffffffffffULL;
		return {o, u16(destination << 8), st0, destination ? 0x12345678 : source, destination ? source : 0x12345678, result};
	}
	bool const mapped = variant >= 147456;
	unsigned const v = mapped ? (variant - 147456) % 32768 : variant % 49152;
	unsigned const selector = mapped ? (v / 8) % 8 : (v / 12) % 8;
	unsigned const pattern = mapped ? (v / 64) % 4 : (v / 96) % 4;
	unsigned const flags = mapped ? (v / 256) % 4 : (v / 384) % 4;
	int const shift = int(mapped ? (v / 1024) % 32 : (v / 1536) % 32) - 16;
	auto o = memory_arithmetic_case(std::array<unsigned, 6>{0, 1, 4, 14, 15, 17}[v % 6] + 18 * ((v / 6) % 2 ? 6 : 1) + 576 * pattern + 4608 * flags);
	o.memory = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[pattern];
	o.opcode = 0x6f00 | u8(o.opcode); ++o.read_cycle;
	if (mapped)
	{
		o.form = 17; o.reg = o.selected = 1; o.address = 0x10 + v % 8; o.literal = o.address;
		o.opcode = 0x6ff8; o.read_cycle = 7; o.st1 &= ~0x4000;
	}
	u64 const a = std::array<u64, 4>{0, 0x7fffffff, 0xff80000000ULL, 0x017fffffffULL}[pattern];
	u64 const b = std::array<u64, 4>{0xffffffffffULL, 0xff7fffffffULL, 0x12345678, 0xff80000000ULL}[pattern];
	u64 const source = selector & 2 ? b : a;
	s64 const value = flags & 1 ? s16(o.memory) : s64(o.memory);
	u64 const memory = u64(value) & 0xffffffffffULL;
	u64 const shifted = shift >= 0 ? (memory << shift) & 0xffffffffffULL : flags & 1 ? u64(value >> -shift) & 0xffffffffffULL : memory >> -shift;
	s64 const left = BIT(source, 39) ? s64(source) - (s64(1) << 40) : s64(source);
	s64 const right = BIT(shifted, 39) ? s64(shifted) - (s64(1) << 40) : s64(shifted);
	s64 const sum = selector & 4 ? left - right : left + right;
	bool const carry = selector & 4 ? u32(source) >= u32(shifted) : u64(u32(source)) + u32(shifted) > 0xffffffffULL;
	bool const overflow = sum < -2147483648LL || sum > 2147483647LL;
	u16 const st0 = (o.st0 & ~0x800) | (carry ? 0x800 : 0) | (overflow ? selector & 1 ? 0x200 : 0x400 : 0);
	u64 const result = u64(overflow && (flags & 2) ? std::clamp(sum, s64(-2147483648LL), s64(2147483647LL)) : sum) & 0xffffffffffULL;
	return {o, u16(0x0c00 | ((selector & 3) << 8) | ((selector & 4) << 3) | (shift & 31)), st0, a, b, result};
}

void tms320c54x_test_state::start_pipeline_shifted_arithmetic(unsigned variant)
{
	if (!variant) { m_shifted_coverage = {}; m_shifted_replays = 0; }
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const bk = profile_is("shifted-bk");
	auto const f = shifted_arithmetic_case(variant, bk); auto const &o = f.operand;
	bool const subc = variant >= 245760;
	u32 const base = o.reg == 6 ? 0x20000 : 0;
	auto &program = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < 24; ++i) program.write_word(base + 0x600 + i, 0xf495);
	program.write_word(base + 0x600, o.opcode);
	if (o.form >= 14) program.write_word(base + 0x601, o.literal);
	if (!subc) program.write_word(base + 0x601 + (o.form >= 14), f.extension);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, o.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, o.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_BK, o.bk);
	for (unsigned ar = 0; ar < 8; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + ar, 0x5007);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x1122); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x3344);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x9abc);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	if (o.address >= 0x60) m_cpu->space(AS_DATA).write_word(o.address, o.memory);
	else m_cpu->set_state_int(bk ? tms320c54x_device::STATE_BK : tms320c54x_device::STATE_AR0 + o.address - 0x10, o.memory);
	m_cpu->set_state_int(STATE_GENPC, base + 0x600); m_cpu->set_pipeline_frontend(true);
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = m_cpu->space(AS_DATA).install_read_tap(0, 0xffff, "shifted_arithmetic_read",
		[this, o](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return;
			expect(at == o.address && word == o.memory && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == o.read_cycle, "Shifted ADD/SUB samples one retained RAM or mapped AR source"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = m_cpu->space(AS_DATA).install_write_tap(0, 0xffff, "shifted_arithmetic_no_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Shifted ADD/SUB has no data writes"); });
	m_pipeline_fetch_tap = program.install_read_tap(base + 0x601, base + 0x601 + (!subc && o.form >= 14), "shifted_arithmetic_extensions",
		[this, f, o, base, subc](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled() || (subc && o.form < 14)) return;
			bool const operation = !subc && at == base + 0x601 + (o.form >= 14);
			expect(word == (operation ? f.extension : o.literal) && m_pipeline_extensions_reads == unsigned(operation && o.form >= 14), "Shifted arithmetic retains ordered address and operation extensions"); ++m_pipeline_extensions_reads; });
	m_phase = 253210; m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

std::array<u64, 2> tms320c54x_test_state::repeat_subc_result(memory_repeat_fixture const &f, unsigned iterations)
{
	u64 accumulator = f.dividend; u16 flags = 0x1804;
	for (unsigned i = 0; i < iterations; ++i)
	{
		s64 const dividend = BIT(accumulator, 39) ? s64(accumulator) - (s64(1) << 40) : s64(accumulator);
		s64 const divisor = f.flags & 1 ? s16(f.divisor) : s64(f.divisor);
		s64 const difference = dividend - divisor * 32768;
		bool const subtract = difference >= 0;
		flags = (flags & ~0x800) | (subtract ? 0x800 : 0);
		if (difference < -2147483648LL || difference > 2147483647LL) flags |= f.destination ? 0x200 : 0x400;
		accumulator = u64(subtract ? difference * 2 + 1 : dividend * 2) & 0xffffffffffULL;
	}
	return {accumulator, flags};
}

tms320c54x_test_state::rptbd_shift_store_fixture tms320c54x_test_state::rptbd_shift_store_case(unsigned v)
{
	rptbd_shift_store_fixture f{};
	f.count = std::array<unsigned, 4>{0, 1, 2, 7}[v % 4]; v /= 4;
	f.form = v % 5; v /= 5; f.kind = v % 4; v /= 4;
	f.shift = std::array<int, 3>{-16, -1, 15}[v % 3]; v /= 3;
	f.flags = v % 4; v /= 4;
	f.source = v % 2 ? 0xff7fff1200ULL : 0x0180001200ULL; v /= 2;
	f.base = v % 2 ? 0x20000 : 0; v /= 2;
	f.compiler = v % 2; v /= 2; f.irq = v % 5; v /= 5;
	f.checkpoint = std::array<unsigned, 3>{4, 8, 12}[v];
	f.mode = std::array<u16, 5>{3, 0x83, 0x8b, 0x93, 0x13}[f.form];
	f.st0 = 0xa600 | (f.form == 4 ? 0 : 8);
	f.st1 = (f.compiler ? 0x4000 : 0) | (f.flags & 2 ? 0x100 : 0) | (f.irq ? 0 : 0x800);
	f.address = f.form >= 1 && f.form <= 3 ? 0x5000 : f.compiler ? 0x3000 + f.mode : (f.st0 & 0x1ff) * 128 + f.mode;
	f.initial_ar3 = f.form >= 1 && f.form <= 3 ? 0x5000 : 0x7103;
	u64 bits = f.source;
	for (int i = 0; i < f.shift; ++i) bits = (bits * 2) & 0xffffffffffULL;
	for (int i = 0; i < -f.shift; ++i) bits = (bits >> 1) | ((f.flags & 2) && (bits & 0x8000000000ULL) ? 0x8000000000ULL : 0);
	u32 stored = u32(bits);
	if (f.flags & 1)
	{
		if (f.flags & 2) stored = u32(std::clamp(s64(bits << 24) >> 24, s64(-2147483648LL), s64(2147483647LL)));
		else if (bits > 0xffffffffULL) stored = 0xffffffffU;
	}
	f.value = stored >> (f.kind & 2 ? 16 : 0);
	f.final_ar3 = f.address == 0x13 ? f.value : f.initial_ar3 + (f.form == 3 ? 1 : f.form == 2 ? -1 : 0);
	return f;
}

void tms320c54x_test_state::start_rptbd_shift_store(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = rptbd_shift_store_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 24; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, 0xf272); program.write_word(f.base + 0x601, 0x605);
	program.write_word(f.base + 0x602, 0x6f00 | f.mode);
	program.write_word(f.base + 0x603, (f.kind & 2 ? 0x0c60 : 0x0c80) | ((f.kind & 1) << 8) | (f.shift & 31));
	program.write_word(f.base + 0x604, 0xe800 | ((f.kind & 1) << 8)); program.write_word(f.base + 0x605, 0x6d94);
	program.write_word(f.base + 0x60f, 0xf4e1);
	int const line = f.irq == 2 || f.irq == 4 ? INPUT_LINE_NMI : 2;
	u32 const vector = f.base + (line == INPUT_LINE_NMI ? 4 : 0x48);
	program.write_word(vector, 0xf495); program.write_word(vector + 1, 0xf4eb);
	if (f.address >= 0x60) data.write_word(f.address, 0xdead);
	m_cpu->set_pipeline_frontend(false); m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, f.flags & 1); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.kind & 1 ? 0x123456789aULL : 0x11112222);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, f.kind & 1 ? 0x11112222 : 0xfedcba9876ULL);
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC, f.count); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, f.irq && line == 2 ? 4 : 0); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x5aa5); m_cpu->set_state_int(tms320c54x_device::STATE_BK, 13);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, i == 3 ? f.initial_ar3 : i == 4 ? 0x6000 : 0x7000 + i);
	m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_rptbd_counts = {}; m_rptbd_return = 0; m_pipeline_replay = false; m_idle_fixture_tick = 0;
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x603, "rptbd_shift_words", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		unsigned const i = m_rptbd_counts.fetches++;
		expect(i < 3 && at == f.base + 0x601 + i && word == (i == 0 ? 0x605 : i == 1 ? 0x6f00 | f.mode : (f.kind & 2 ? 0x0c60 : 0x0c80) | ((f.kind & 1) << 8) | (f.shift & 31)), "RPTBD retains its endpoint and complete shifted-store delay exactly once"); });
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "rptbd_shift_reads", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		expect(f.irq && at == 0x2fff && !m_rptbd_counts.stack_reads++ && m_rptbd_counts.stack_writes == 1 && word == m_rptbd_return, "Shifted store has no destination read; ISR reads one original continuation"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "rptbd_shift_writes", [this, f](offs_t at, u16 &word, u16) {
		if (machine().side_effects_disabled()) return;
		if (at == 0x2fff) { expect(f.irq && !m_rptbd_counts.stack_writes++ && word >= 0x604 && word <= 0x60f && m_rptbd_counts.delay_writes == 1, "Interrupt stacks one continuation after the complete delay store"); m_rptbd_return = word; return; }
		expect(at == f.address && word == f.value && !m_rptbd_counts.delay_writes++ && m_cpu->pipeline_cycles() == 9, "Shifted store publishes the Execute-live source to its retained delay address once"); });
	m_phase = 310400; m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE);
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

void tms320c54x_test_state::start_memory_immediate_multiply(unsigned variant)
{
	if (!variant) { m_memory_immediate_coverage = {}; m_memory_immediate_restores = 0; }
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = memory_immediate_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, (f.accumulate ? 0x6400 : 0x6200) | (f.bsrc ? 0x200 : 0) | (f.bdst ? 0x100 : 0) | f.mode);
	program.write_word(f.base + 0x601, f.words == 3 ? f.extension : f.literal); if (f.words == 3) program.write_word(f.base + 0x602, f.literal); program.write_word(f.base + 0x600 + f.words, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, f.pmst); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x9abc);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1400); m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x140c); m_cpu->set_state_int(tms320c54x_device::STATE_BK, f.form == 7 ? 13 : 0); data.write_word(f.address, f.word);
	m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "memory_immediate_read", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(at == f.address && word == f.word && !m_pipeline_operand_reads && m_cpu->pipeline_cycles() == f.read, "Memory/immediate MPY retains one Read-owned source"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "memory_immediate_no_write", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "Memory/immediate MPY has no data-memory write"); });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x600 + f.words - 1, "memory_immediate_literals", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; unsigned const i = m_pipeline_extensions_reads++; expect(i < f.words - 1 && at == f.base + 0x601 + i && word == (f.words == 3 && !i ? f.extension : f.literal), "MPY retains address then literal exactly once"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254340;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::delay_multiply_fixture tms320c54x_test_state::delay_multiply_case(unsigned variant, bool mac, bool rounded)
{
	unsigned const group = mac || rounded ? variant / 18432 : 0;
	variant %= 18432;
	unsigned const kind = variant % 6, form = (variant / 6) % 4, destination = (variant / 24) % 2, pair = (variant / 48) % 16, flags = (variant / 768) % 4;
	u16 const memory = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[pair % 4], t = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[pair / 4];
	u32 const caller = (variant / 3072) % 2 ? 0x20000 : 0, target = kind == 4 ? caller ^ 0x20000 : caller;
	s64 const product = s64(s16(memory)) * s16(t) * (flags & 1 ? 2 : 1);
	u64 const result = u64(product > 0x7fffffffLL && (flags & 2) ? 0x7fffffffLL : product) & 0xffffffffffULL;
	unsigned const read = delay_multiply_fixture::operand_read(kind, form);
	delay_multiply_fixture f{kind, form, destination, flags, std::array<unsigned, 3>{3, read - 1, read}[variant / 6144], memory, t,
		std::array<u16, 4>{0x5000, 0x5000, u16(mac ? 0x15 : 0x17), 0x5003}[form], caller, target,
		kind == 1 ? 0 : kind == 2 ? 1 : 0x123456789aULL, 0xfedcba9876ULL, result};
	if (mac)
	{
		f.accumulate = true; f.operation = group % 4; f.flags |= group / 4 ? 4 : 0;
		s64 const value = f.arithmetic();
		f.result = u64(f.overflow() && (f.flags & 2) ? std::clamp(value, s64(-2147483648LL), s64(2147483647LL)) : value) & 0xffffffffffULL;
	}
	if (rounded)
	{
		f.rounded = true; f.flags |= group ? 4 : 0;
		s64 const value = f.arithmetic();
		f.result = u64(f.flags & 2 ? std::clamp(value, s64(-2147483648LL), s64(2147483647LL)) : value) & 0xffffffffffULL;
	}
	return f;
}

tms320c54x_test_state::delay_multiply_fixture tms320c54x_test_state::delay_accumulator_multiply_case(unsigned variant)
{
	unsigned v = variant;
	delay_multiply_fixture f{};
	f.kind = v % 6; v /= 6; f.form = v % 3; v /= 3;
	f.operation = v % 4; v /= 4; unsigned const pattern = v % 8; v /= 8;
	f.flags = v % 16; v /= 16; f.caller = v % 2 ? 0x20000 : 0; v /= 2;
	f.target = f.kind == 4 ? f.caller ^ 0x20000 : f.caller;
	f.accumulator_source = true; f.destination = 1; f.t = 0x5aa5;
	f.source = f.form ? 0x5000 : 0x1062;
	s32 const high = std::array<s32, 8>{0, 1, -1, 32767, -32768, 65535, -65536, 0x1234}[pattern];
	f.a = f.kind == 1 ? 0 : f.kind == 2 ? 1 : (pattern & 1 ? 0xfe00000000ULL : 0) | ((u64(high) & 0x1ffff) << 16) | (0x1234 + pattern);
	f.b = std::array<u64, 8>{0, 1, 0x7fffffff, 0xff80000000ULL, 0x017fffffffULL, 0xff7fffffffULL, 0x1234567833ULL, 0x80aa001234ULL}[pattern];
	f.memory = std::array<u16, 8>{0, 1, 0xffff, 0x7fff, 0x8000, 0x8000, 0xffff, 0x5678}[pattern];
	f.checkpoint = std::array<unsigned, 3>{3, f.read() - 1, f.read()}[v];
	s64 const result = f.arithmetic();
	f.result = u64(f.flags & 2 ? std::clamp(result, s64(-2147483648LL), s64(2147483647LL)) : result) & 0xffffffffffULL;
	return f;
}

tms320c54x_test_state::delay_multiply_fixture tms320c54x_test_state::bd_long_accumulator_case(unsigned v)
{
	delay_multiply_fixture f{};
	f.extended_accumulator = f.accumulator_source = true; f.destination = 1;
	f.form = v % 3; v /= 3; f.address_ar = v % 8; v /= 8;
	f.operation = v % 4; v /= 4; unsigned const pattern = v % 8; v /= 8;
	f.flags = v % 16; v /= 16; f.caller = v % 2 ? 0x20000 : 0; v /= 2;
	f.target = f.caller; f.t = 0x5aa5;
	f.source = f.form == 2 ? 0x5000 : f.form ? 0x3b08 : 0x5003;
	s32 const high = std::array<s32,8>{0,1,-1,32767,-32768,65535,-65536,0x1234}[pattern];
	f.a = (pattern & 1 ? 0xfe00000000ULL : 0) | ((u64(high) & 0x1ffff) << 16) | (0x1234 + pattern);
	f.b = std::array<u64,8>{0,1,0x7fffffff,0xff80000000ULL,0x017fffffffULL,0xff7fffffffULL,0x1234567833ULL,0x80aa001234ULL}[pattern];
	f.memory = std::array<u16,8>{0,1,0xffff,0x7fff,0x8000,0x8000,0xffff,0x5678}[pattern];
	f.checkpoint = std::array<unsigned,4>{3,5,8,9}[v % 4];
	s64 const result = f.arithmetic();
	f.result = u64(f.flags & 2 ? std::clamp(result,s64(-2147483648LL),s64(2147483647LL)) : result) & 0xffffffffffULL;
	return f;
}

void tms320c54x_test_state::start_delay_multiply(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const mac = profile_is("mac-delay");
	bool const square = profile_is("square-delay");
	bool const memory_square = profile_is("squr-delay");
	bool const long_acc = profile_is("bd-maca-long");
	bool const acc = square || memory_square || long_acc || profile_is("maca-delay");
	bool const rounded = profile_is("mpyr-delay");
	auto const f = long_acc ? bd_long_accumulator_case(variant) : memory_square ? delay_memory_square_case(variant) : square ? delay_square_case(variant) : acc ? delay_accumulator_multiply_case(variant) : delay_multiply_case(variant, mac, rounded); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.caller + 0x600 + i, 0xf495);
	program.write_word(f.caller + 0x600, f.kind == 4 ? 0xfb80 | (f.target >> 16) : std::array<u16, 6>{0xf273, 0xfa45, 0xfa45, 0xf274, 0, 0xfe00}[f.kind]);
	if (f.kind != 5) program.write_word(f.caller + 0x601, 0x700);
	program.write_word(f.caller + 0x600 + f.delay(), memory_square ? 0x2600 | (f.destination << 8) | (f.form == 0 ? 0x62 : f.form == 1 ? 0x82 : 0x92) : square ? (0x3800 | (f.operation << 9) | (f.destination << 8) | (f.form == 0 ? 0x62 : f.form == 1 ? 0x82 : 0x92)) : acc ? (0x3100 + f.operation * 0x200) | (f.form == 0 ? 0x62 : f.form == 1 ? 0x82 : 0x92) : (mac ? 0x2800 + f.operation * 0x200 : rounded ? 0x2200 : 0x2000) | (f.destination << 8) | (f.form == 0 ? rounded ? 0x92 : 0x82 : f.form == 3 ? 0xe2 : 0xf8));
	if (long_acc) program.write_word(f.caller + 0x602,(0x3100 + f.operation * 0x200) | (f.form == 2 ? 0xf8 : 0xe0 | f.address_ar));
	if (f.form && !acc) program.write_word(f.caller + 0x601 + f.delay(), f.form == 3 ? 3 : f.source);
	if (long_acc) program.write_word(f.caller + 0x603,f.extension());
	for (u32 at : {f.caller + 0x604, f.target + 0x700}) { program.write_word(at, 0x880f | (f.destination << 8)); program.write_word(at + 1, 0xf4e1); }
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, f.flags & 4 ? 2 : 0); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, acc ? 0xa020 : 0xa008); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, (acc ? 0x800 | (f.flags & 8 ? 0x100 : 0) : 0x900) | (f.flags & 1 ? 0x40 : 0) | (f.flags & 2 ? 0x200 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b); m_cpu->set_state_int(tms320c54x_device::STATE_T, f.t); m_cpu->set_state_int(tms320c54x_device::STATE_TRN, 0x2468);
	if (long_acc) { m_cpu->set_state_int(tms320c54x_device::STATE_A,f.a ^ 0xffffffffffULL); m_cpu->set_state_int(tms320c54x_device::STATE_B,f.b ^ 0xffffffffffULL); }
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x5000); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000); data.write_word(0x3000, 0x700); data.write_word(f.source, f.memory);
	if (long_acc) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.address_ar,0x5000);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_register_writes = m_fast_stack_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "delay_multiply_read", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == 0x3000) { expect(f.kind == 5 && word == 0x700 && !m_fast_stack_reads++, "RETD owns its separate one-word return frame"); return; } if (at != f.source || word != f.memory || m_pipeline_operand_reads || m_cpu->pipeline_cycles() != f.read()) osd_printf_info("delay_multiply_read: case=%u address=%04x word=%04x cycle=%llu expected=%u\n", m_pipeline_peripheral_case, unsigned(at), word, (unsigned long long)m_cpu->pipeline_cycles(), f.read()); expect(at == f.source && word == f.memory && !m_pipeline_operand_reads++ && m_cpu->pipeline_cycles() == f.read(), "Delay MPY reads one retained RAM or AR operand"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "delay_multiply_write", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (at == 0x2fff || at == 0x2ffe) { unsigned const i = m_pipeline_register_writes++; expect((f.kind == 3 || f.kind == 4) && i < (f.kind == 4 ? 2 : 1) && at == 0x2fff - i && word == (i ? f.caller >> 16 : 0x604), "Delayed call owns its continuation and optional caller-page frame"); return; } expect(at == 0x0f && word == u16(f.result) && !m_pipeline_store_writes++ && m_pipeline_operand_reads == 1 && m_cpu->pipeline_cycles() >= f.read() + 2, "The selected callee observes the completed multiply once"); });
	m_pipeline_fetch_tap = program.install_read_tap(f.caller + 0x600, f.caller + 0x604, "delay_multiply_literal", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled() || (f.accumulator_source && !f.extended_accumulator) || (!f.form && !f.extended_accumulator) || at != f.caller + 0x601 + f.delay()) return; expect(word == f.extension() && !m_pipeline_extensions_reads++ && m_cpu->pipeline_cycles() == f.literal_clock(), "Delay multiply fetches its retained address literal once"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254540;
	m_cpu->set_state_int(STATE_GENPC, f.caller + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}


























tms320c54x_test_state::delay_load_fixture tms320c54x_test_state::delay_shift_alu_case(unsigned variant)
{
	unsigned const operation = variant % 4, v = variant / 4;
	auto f = delay_load_case(v, true);
	u64 const shifted = f.result, source = operation & 1 ? f.b : f.a;
	s64 const left = source & 0x8000000000ULL ? s64(source) - 0x10000000000LL : s64(source);
	s64 const right = shifted & 0x8000000000ULL ? s64(shifted) - 0x10000000000LL : s64(shifted);
	s64 const result = operation & 2 ? left - right : left + right;
	bool const carry = operation & 2 ? u32(source) >= u32(shifted) : u64(u32(source)) + u32(shifted) > 0xffffffffULL;
	bool const overflow = result < -2147483648LL || result > 2147483647LL;
	f.st0 = 0xa020 | ((v / 1152) % 4 << 9);
	f.final_st0 = (f.st0 & ~0x800) | (carry ? 0x800 : 0) | (overflow ? f.destination ? 0x200 : 0x400 : 0);
	f.result = u64(f.flags & 2 ? std::clamp(result, s64(-2147483648LL), s64(2147483647LL)) : result) & 0xffffffffffULL;
	f.literal = 0x0c00 | (operation & 2 ? 0x20 : 0) | ((operation & 1) << 9) | (f.destination << 8) | (f.literal & 31);
	return f;
}



tms320c54x_test_state::delay_compare_fixture tms320c54x_test_state::delay_compare_case(unsigned v)
{
	delay_compare_fixture f{};
	f.kind = v % 3; f.bitfield = (v / 3) % 2; f.form = (v / 6) % 3;
	unsigned const pair = (v / 18) % 16;
	f.memory = std::array<u16, 4>{0, 1, 0x8000, 0xffff}[pair % 4];
	f.literal = std::array<u16, 4>{0, 1, 0x8000, 0xf495}[pair / 4];
	f.initial_tc = (v / 288) % 2; f.prefix = (v / 576) % 2 ? 2 : 0;
	bool const branch_tc = f.prefix ? bool(f.memory & 1) : f.initial_tc;
	f.taken = !f.kind || (f.kind == 1 ? branch_tc : !branch_tc);
	f.final_tc = f.bitfield ? bool(f.memory & f.literal) : f.memory == f.literal;
	f.interrupt = (v / 1152) % 3; f.base = (v / 3456) % 2 ? 0x20000 : 0;
	f.read = (f.kind ? 9 : 8) + f.prefix; f.literal_clock = (f.kind ? 6 : 5) + f.prefix;
	f.checkpoint = std::array<unsigned, 4>{f.read - 1, f.read, f.read + 1, f.read + 3}[v / 6912];
	return f;
}

void tms320c54x_test_state::start_delay_compare(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant; auto const f = delay_compare_case(variant);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 64; ++i) { program.write_word(f.base + 0x600 + i, 0xf495); program.write_word(f.base + 0x700 + i, 0xf495); program.write_word(f.base + i, 0xf495); }
	if (f.prefix) { program.write_word(f.base + 0x600, 0x6102); program.write_word(f.base + 0x601, 1); }
	program.write_word(f.base + 0x600 + f.prefix, f.kind == 0 ? 0xf273 : f.kind == 1 ? 0xfa30 : 0xfa20); program.write_word(f.base + 0x601 + f.prefix, 0x700);
	program.write_word(f.base + 0x602 + f.prefix, (f.bitfield ? 0x6100 : 0x6000) | (f.form == 2 ? 0x92 : 2)); program.write_word(f.base + 0x603 + f.prefix, f.literal);
	program.write_word(f.base + 0x604 + f.prefix, 0xe922); program.write_word(f.base + 0x605 + f.prefix, 0xf4e1); program.write_word(f.base + 0x700, 0xe911); program.write_word(f.base + 0x701, 0xf4e1);
	program.write_word(f.base + 4, 0xf4eb); program.write_word(f.base + 0x40, 0xf4eb); data.write_word(0x1002, f.memory);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 1);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0xae20 | (f.initial_tc ? 0x1000 : 0)); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x100 | (f.form == 1 ? 0x4000 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x123456789aULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, 0x55); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0x5aa5);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1000); m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x1002);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = m_vector_acks = 0;
	m_pipeline_operand_tap = data.install_read_tap(0x1002, 0x1002, "delay_compare_read", [this, f](offs_t, u16 &word, u16) { if (machine().side_effects_disabled()) return; unsigned const clock = f.prefix && !m_pipeline_operand_reads ? 6 : f.read; if (m_cpu->pipeline_cycles() != clock) osd_printf_info("delay_compare_read: case=%u actual=%llu expected=%u reads=%u\n", m_pipeline_peripheral_case, (unsigned long long)m_cpu->pipeline_cycles(), clock, m_pipeline_operand_reads); expect(word == f.memory && m_pipeline_operand_reads < (f.prefix ? 2U : 1U) && m_cpu->pipeline_cycles() == clock, "Delay comparison reads its retained source once after any prefix producer"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0x1002, 0x1002, "delay_compare_no_write", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) { ++m_pipeline_store_writes; expect(false, "Delay CMPM BITF never writes the operand"); } });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x600, f.base + 0x605 + f.prefix, "delay_compare_literals", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; if (f.prefix && at == f.base + 0x601) { expect(word == 1 && !m_pipeline_extensions_reads, "Prefix BITF owns its mask literal"); ++m_pipeline_extensions_reads; } else if (at == f.base + 0x601 + f.prefix) { expect(word == 0x700 && m_pipeline_extensions_reads == unsigned(bool(f.prefix)), "Delayed branch owns its target literal"); ++m_pipeline_extensions_reads; } else if (at == f.base + 0x603 + f.prefix) { expect(word == f.literal && m_pipeline_extensions_reads == 1 + unsigned(bool(f.prefix)) && m_cpu->pipeline_cycles() == f.literal_clock, "Two-word comparison owns its delay literal, including opcode-shaped values"); ++m_pipeline_extensions_reads; } });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 254980;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

tms320c54x_test_state::parallel_load_fixture tms320c54x_test_state::parallel_arithmetic_case(unsigned variant)
{
	unsigned const extent = 67584, form = variant / extent;
	auto c = parallel_load_case(variant % extent + (form % 4) * extent);
	c.form = form; auto &f = c.memory;
	u64 const old_other = f.destination ? c.a : c.b;
	s64 const other = old_other & 0x8000000000ULL ? s64(old_other) - 0x10000000000LL : s64(old_other);
	s64 const memory = s64(f.st1 & 0x100 ? s16(f.xword) : s32(f.xword)) * 65536;
	s64 result = form >= 4 ? memory - other : other + memory;
	bool const carry = form >= 4 ? u32(memory) >= u32(old_other) : u64(u32(memory)) + u32(old_other) > 0xffffffffULL;
	f.result_st0 = (f.st0 & ~0x800) | (carry ? 0x800 : 0);
	if (result < -2147483648LL || result > 2147483647LL) { f.result_st0 |= f.destination ? 0x200 : 0x400; if (f.st1 & 0x200) result = std::clamp(result, s64(-2147483648LL), s64(2147483647LL)); }
	c.result_a = f.destination ? c.a : u64(result) & 0xffffffffffULL;
	c.result_b = f.destination ? u64(result) & 0xffffffffffULL : c.b;
	return c;
}

tms320c54x_test_state::parallel_load_fixture tms320c54x_test_state::parallel_multiply_case(unsigned variant)
{
	auto c = parallel_load_case(variant);
	auto &f = c.memory;
	c.t = std::array<u16, 8>{0, 1, 0xffff, 0x8000, 0x7fff, 0x1234, 0x5555, 0xaaaa}[(variant / 8) % 8];
	f.st1 = (f.st1 & ~0x40) | ((variant / 64) % 2 ? 0x40 : 0);
	f.pmst = (f.pmst & ~2) | ((variant / 128) % 2 ? 2 : 0);
	s64 const product = s64(s16(f.xword)) * s16(c.t) * (f.st1 & 0x40 ? 2 : 1);
	bool const overflow = product < -2147483648LL || product > 2147483647LL;
	u64 const result = u64(f.st1 & 0x200 ? std::clamp(product, s64(-2147483648LL), s64(2147483647LL)) : product) & 0xffffffffffULL;
	c.result_a = f.destination ? c.a : result;
	c.result_b = f.destination ? result : c.b;
	f.result_st0 = f.st0 | (overflow ? f.destination ? 0x200 : 0x400 : 0);
	return c;
}

tms320c54x_test_state::sqdst_fixture tms320c54x_test_state::sqdst_case(unsigned variant, bool lms)
{
	bool const numeric = variant >= 9216;
	unsigned const n = numeric ? variant - 9216 : variant;
	sqdst_fixture c{};
	auto &f = c.memory;
	f = dual_multiply_case(numeric ? 0 : n % 256 + 512 * (n / 256), false);
	static constexpr std::array<u16, 8> words{0, 1, 0xffff, 0x8000, 0x7fff, 0x1234, 0x5555, 0xaaaa};
	if (numeric)
	{
		f.x = 0; f.y = 1; f.xaddress = 0x1000; f.yaddress = 0x1100;
		f.initial = {0x1000, 0x1100, 0x1200, 0x1300}; f.final = f.initial;
		f.xword = words[n % 8]; f.yword = words[(n / 8) % 8];
		f.flags = (n / 4096) % 32; f.snapshot = n / 131072;
		f.base = (n / 64) & 1 ? 0x20000 : 0; f.linear = true;
		f.st0 = 0xa002 | (f.flags & 16 ? 0x1e00 : 0);
		f.st1 = 0x800 | (f.flags & 1 ? 0x40 : 0) | (f.flags & 2 ? 0x200 : 0) | (f.flags & 8 ? 0x100 : 0);
		f.pmst = f.flags & 4 ? 2 : 0;
	}
	c.a = std::array<u64, 8>{0, 0x007fff0000ULL, 0x0080000000ULL, 0x00ffff0000ULL, 0x0100000000ULL, 0x017fff0000ULL, 0xff80000000ULL, 0xffffffffffULL}[(n / (numeric ? 64 : 128)) % 8];
	c.b = std::array<u64, 8>{0, 1, 0x7fffffff, 0xff80000000ULL, 0x0180000000ULL, 0xfe7fffffffULL, 0x123456789aULL, 0xffffffffffULL}[(n / 512) % 8];
	s64 const high = s64((c.a >> 16) & 0xffff) - (c.a & (u64(1) << 32) ? 65536 : 0);
	s64 square = high * high * (f.flags & 1 ? 2 : 1);
	if ((f.flags & 7) == 7) square = std::min(square, s64(2147483647LL));
	s64 const sum = (c.b & (u64(1) << 39) ? s64(c.b) - (s64(1) << 40) : s64(c.b)) + square;
	s64 const difference = (s64(f.flags & 8 ? s16(f.xword) : s32(f.xword)) - s64(f.flags & 8 ? s16(f.yword) : s32(f.yword))) * 65536;
	bool const ova = difference < -2147483648LL || difference > 2147483647LL;
	bool const ovb = sum < -2147483648LL || sum > 2147483647LL;
	c.result_a = u64((f.flags & 2) ? std::clamp(difference, s64(-2147483648LL), s64(2147483647LL)) : difference) & 0xffffffffffULL;
	c.result_b = u64((f.flags & 2) ? std::clamp(sum, s64(-2147483648LL), s64(2147483647LL)) : sum) & 0xffffffffffULL;
	c.result_st0 = (f.st0 & ~0x800) | (f.xword >= f.yword ? 0x800 : 0) | (ova ? 0x400 : 0) | (ovb ? 0x200 : 0);
	if (lms)
	{
		s64 const operand = s64(f.flags & 8 ? s16(f.xword) : s32(f.xword)) * 65536 + 32768;
		s64 const a = (c.a & 0x8000000000ULL ? s64(c.a)-0x10000000000LL : s64(c.a)) + operand;
		s64 product = s64(s16(f.xword)) * s16(f.yword) * (f.flags & 1 ? 2 : 1);
		if ((f.flags & 7) == 7) product = std::clamp(product,s64(-2147483648LL),s64(2147483647LL));
		s64 const b = (c.b & 0x8000000000ULL ? s64(c.b)-0x10000000000LL : s64(c.b)) + product;
		bool const carry = u64(u32(c.a)) + u32(operand) > 0xffffffffULL;
		c.result_st0 = (f.st0 & ~0x800) | (carry ? 0x800 : 0) | (a < -2147483648LL || a > 2147483647LL ? 0x400 : 0) | (b < -2147483648LL || b > 2147483647LL ? 0x200 : 0);
		c.result_a = u64(f.flags & 2 ? std::clamp(a,s64(-2147483648LL),s64(2147483647LL)) : a) & 0xffffffffffULL;
		c.result_b = u64(f.flags & 2 ? std::clamp(b,s64(-2147483648LL),s64(2147483647LL)) : b) & 0xffffffffffULL;
	}
	return c;
}

void tms320c54x_test_state::start_sqdst(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const lms = !strncmp(machine().options().bios(), "lms", 3);
	auto const c = sqdst_case(variant, lms); auto const &f = c.memory;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, (lms ? 0xe100 : 0xe200) | (f.x << 4) | f.y); program.write_word(f.base + 0x601, 0xf4e1);
	if (f.xaddress >= 0x60) data.write_word(f.xaddress, f.xword);
	if (f.yaddress >= 0x60) data.write_word(f.yaddress, f.yword);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, f.pmst); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, c.a ^ 0xffffffffffULL); m_cpu->set_state_int(tms320c54x_device::STATE_B, c.b ^ 0xffffffffffULL); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xbeef);
	for (unsigned i = 0; i < 4; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR2 + i, f.initial[i]);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 3); m_cpu->set_state_int(tms320c54x_device::STATE_BK, f.linear ? 0 : 16);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_pipeline_operand_reads = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "sqdst_reads", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(m_pipeline_operand_reads < 2 && at == (m_pipeline_operand_reads ? f.yaddress : f.xaddress) && word == (m_pipeline_operand_reads ? f.yword : f.xword) && m_cpu->pipeline_cycles() == 5, "SQDST reads both retained operands once"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "sqdst_no_write", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) expect(false, "SQDST has no data write"); });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x600, f.base + 0x600, "sqdst_fetch", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) { expect(!m_pipeline_extensions_reads, "SQDST opcode fetched once"); ++m_pipeline_extensions_reads; } });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62142;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

std::array<u64, 2> tms320c54x_test_state::addc_expected(u64 source, u16 word, u16 st0, u16 st1, bool b, unsigned family)
{
	s64 const left = source & 0x8000000000ULL ? s64(source) - (s64(1) << 40) : s64(source);
	u64 const increment = u64(word) + (family ? 0 : (st0 >> 11) & 1);
	s64 const sum = family == 2 ? left - s64(increment) : left + s64(increment);
	bool const overflow = sum < -2147483648LL || sum > 2147483647LL;
	bool const carry = family == 2 ? (source % 0x100000000ULL) >= increment : (source % 0x100000000ULL) + increment >= 0x100000000ULL;
	u16 const status = (st0 & ~u16(0x800)) | (carry ? 0x800 : 0) | (overflow ? b ? 0x200 : 0x400 : 0);
	u64 const result = u64(overflow && (st1 & 0x200) ? (sum < 0 ? -2147483648LL : 2147483647LL) : sum) & 0xffffffffffULL;
	return {result, status};
}

tms320c54x_test_state::addc_fixture tms320c54x_test_state::addc_case(unsigned v, unsigned family)
{
	static constexpr std::array<u64, 8> accumulators{0, 0xffffffffULL, 0x7fffffffULL, 0xff80000000ULL, 0x7fffffffffULL, 0x8000000000ULL, 0xffff, 0x123456789aULL};
	static constexpr std::array<u16, 8> words{0, 1, 0x7fff, 0x8000, 0xffff, 0xaaaa, 0x5555, 0xfffe};
	addc_fixture f{};
	unsigned form, pattern = 0, flags = 0, destination = 0, page = 0, mode = 0;
	bool const mapped = v >= 73728;
	if (!mapped)
	{
		form = v % 6; v /= 6; f.word = words[v % 8]; v /= 8; pattern = v % 8; v /= 8;
		flags = v % 8; v /= 8; destination = v % 2; v /= 2; f.late = v % 2; v /= 2; page = v % 2; v /= 2; f.snapshot = v;
	}
	else
	{
		v -= 73728; f.address = 8 + v % 16; v /= 16; form = v % 3; v /= 3;
		pattern = v % 4; v /= 4; flags = v % 8; v /= 8; destination = v % 2; v /= 2;
		f.late = v % 2; v /= 2; page = v % 2; v /= 2; f.snapshot = v;
	}
	f.a = destination ? 0x123456789aULL : accumulators[pattern]; f.b = destination ? accumulators[pattern] : 0xfeabcd9876ULL;
	f.t = words[pattern]; f.trn = words[(pattern + 3) % 8]; f.base = page ? 0x20000 : 0;
	for (unsigned i = 0; i < 8; ++i) f.initial[i] = 0x1200 + 0x100 * i;
	f.initial[0] = 1;
	f.st0 = 0x1000 | (mapped && !form ? 0 : 0x20) | (flags & 1 ? 0x800 : 0) |
		(pattern & 1 ? (destination ? 0x400 : 0x200) : 0) | (pattern & 2 ? (destination ? 0x200 : 0x400) : 0);
	f.st1 = 0x800 | (flags & 2 ? 0x100 : 0) | (flags & 4 ? 0x200 : 0);
	if (mapped)
	{
		mode = form == 0 ? f.address : form == 1 ? 0xf8 : 0x83;
		f.extension = form == 1; f.literal = f.address;
		if (form == 2) f.initial[3] = f.address;
		if (mapped) f.word = f.address < 14 ? u16((f.address < 11 ? f.a : f.b) >> (16 * ((f.address - 8) % 3))) & ((f.address - 8) % 3 == 2 ? 0xff : 0xffff) : f.address == 14 ? f.t : f.address == 15 ? f.trn : f.initial[f.address - 16];
	}
	else
	{
		mode = std::array<unsigned, 6>{2, 2, 0xf8, 0x93, 0xe3, 0xd3}[form];
		f.extension = form == 2 || form == 4; f.literal = form == 4 ? 2 : 0x1002;
		f.address = form == 1 ? 0x3002 : form == 5 ? 0x100c : 0x1002;
		f.initial[3] = form == 4 ? 0x1000 : f.address; if (form == 1) f.st1 |= 0x4000; if (form == 5) f.bk = 13;
	}
	f.next_ar3 = !mapped && form == 3 ? 0x1003 : !mapped && form == 5 ? 0x1000 : f.initial[3];
	f.opcode = (family == 1 ? 0x0200 : family == 2 ? 0x0a00 : 0x0600) | (destination << 8) | mode; f.read_clock = f.extension ? 6 : 5;
	f.after_a = f.a; f.after_b = f.b;
	if (mapped && f.address < 14)
	{
		unsigned const part = (f.address - 8) % 3; u64 const mask = part == 2 ? 0xff : 0xffff;
		u64 &source = f.address < 11 ? f.after_a : f.after_b;
		source = (source & ~(mask << (16 * part))) | ((0xa55aULL & mask) << (16 * part));
	}
	if (f.late) (destination ? f.after_b : f.after_a) = accumulators[(pattern + 1) % 8];
	f.execute_st0 = f.st0 ^ (f.late ? 0x800 : 0);
	u64 const source = destination ? f.after_b : f.after_a;
	auto const expected = addc_expected(source, f.word, f.execute_st0, f.st1, destination, family);
	f.final_st0 = expected[1]; f.final_a = destination ? f.after_a : expected[0]; f.final_b = destination ? expected[0] : f.after_b;
	return f;
}

tms320c54x_test_state::addc_fixture tms320c54x_test_state::mapped_shift16_case(unsigned variant)
{
	unsigned const extent = 36864, selector = variant / extent, v = variant % extent;
	auto f = addc_case(73728 + v, 1);
	bool const source_b = selector & 1, subtract = selector & 2, destination_b = BIT(f.opcode, 8);
	if (source_b != destination_b) std::swap(f.a, f.b);
	if (f.address < 14) { unsigned const part = (f.address - 8) % 3; f.word = u16((f.address < 11 ? f.a : f.b) >> (16 * part)) & (part == 2 ? 0xff : 0xffff); }
	f.after_a = f.a; f.after_b = f.b;
	if (f.address < 14) { unsigned const part = (f.address - 8) % 3; u64 const mask = part == 2 ? 0xff : 0xffff; u64 &source = f.address < 11 ? f.after_a : f.after_b; source = (source & ~(mask << (16 * part))) | ((0xa55aULL & mask) << (16 * part)); }
	if (f.late) (source_b ? f.after_b : f.after_a) = std::array<u64, 5>{0, 0xffffffffULL, 0x7fffffffULL, 0xff80000000ULL, 0x7fffffffffULL}[1 + (v / 48) % 4];
	u64 const source = source_b ? f.after_b : f.after_a;
	s64 const left = source & 0x8000000000ULL ? s64(source) - 0x10000000000LL : s64(source);
	s64 const right = s64(f.st1 & 0x100 ? s16(f.word) : s32(f.word)) * 65536;
	s64 result = subtract ? left - right : left + right;
	bool const carry = subtract ? u32(source) >= u32(right) : u64(u32(source)) + u32(right) > 0xffffffffULL;
	f.final_st0 = f.execute_st0;
	if (carry != subtract) f.final_st0 = (f.final_st0 & ~0x800) | (carry ? 0x800 : 0);
	if (result < -2147483648LL || result > 2147483647LL) { f.final_st0 |= destination_b ? 0x200 : 0x400; if (f.st1 & 0x200) result = std::clamp(result, s64(-2147483648LL), s64(2147483647LL)); }
	f.final_a = destination_b ? f.after_a : u64(result) & 0xffffffffffULL;
	f.final_b = destination_b ? u64(result) & 0xffffffffffULL : f.after_b;
	f.opcode = (subtract ? 0x4000 : 0x3c00) | (unsigned(source_b) << 9) | (unsigned(destination_b) << 8) | u8(f.opcode);
	return f;
}

tms320c54x_test_state::addc_fixture tms320c54x_test_state::mapped_logic_case(unsigned variant)
{
	unsigned const extent = 36864, operation = variant / extent, v = variant % extent;
	auto f = addc_case(73728 + v, 1);
	bool const b = BIT(f.opcode, 8); u64 const source = b ? f.after_b : f.after_a;
	u64 const result = operation == 0 ? source & f.word : operation == 1 ? source | f.word : source ^ f.word;
	f.final_a = b ? f.after_a : result; f.final_b = b ? result : f.after_b; f.final_st0 = f.execute_st0;
	f.opcode = 0x1800 | (operation << 9) | (unsigned(b) << 8) | u8(f.opcode);
	return f;
}

void tms320c54x_test_state::start_addc(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const arithmetic = profile_is("unsigned-map");
	bool const shift16 = profile_is("shift16-map");
	bool const logic = profile_is("logic-map");
	unsigned const extent = shift16 || logic ? 36864 : 110592;
	auto const f = logic ? mapped_logic_case(variant) : shift16 ? mapped_shift16_case(variant) : addc_case(variant % extent, arithmetic ? 1 + variant / extent : 0);
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 16; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, f.opcode); if (f.extension) program.write_word(f.base + 0x601, f.literal);
	program.write_word(f.base + 0x601 + unsigned(f.extension), 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0); m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b); m_cpu->set_state_int(tms320c54x_device::STATE_T, f.t); m_cpu->set_state_int(tms320c54x_device::STATE_TRN, f.trn);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x3000); m_cpu->set_state_int(tms320c54x_device::STATE_BK, f.bk); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0);
	for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, f.initial[i]);
	if (f.address >= 0x60) data.write_word(f.address, f.word);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "unsigned_sample", [this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!m_pipeline_operand_reads++ && at == f.address && word == f.word && m_cpu->pipeline_cycles() == f.read_clock, "Unsigned arithmetic retains one real source in Read"); });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "unsigned_no_write", [this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) { ++m_pipeline_store_writes; expect(false, "Unsigned arithmetic writes no data memory"); } });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x601, "unsigned_literal", [this, f](offs_t, u16 &word, u16) { if (!machine().side_effects_disabled() && f.extension) { expect(!m_pipeline_extensions_reads++ && word == f.literal, "Unsigned arithmetic fetches its address extension once"); } });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 62156;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}





tms320c54x_test_state::mac_smem_fixture tms320c54x_test_state::mac_smem_case(unsigned variant, bool bk, bool square)
{
	unsigned const forms = bk ? 2 : 3, selectors = square ? 4 : 8, v = variant % (forms * selectors * 1024), form = v % forms, selector = (v / forms) % selectors, pair = (v / (forms * selectors)) % 64, flags = v / (forms * selectors * 64);
	unsigned const operation = selector / 2, tp = pair / 8, mp = pair % 8;
	u16 const t = std::array<u16, 8>{0, 1, 0xffff, 0x7fff, 0x8000, 0x8001, 0x1234, 0xabcd}[tp];
	u16 const memory = std::array<u16, 8>{0, 1, 0xffff, 0x7fff, 0x8000, 0x8001, 0x5678, 0xfedc}[mp];
	u64 const source = std::array<u64, 8>{0, 1, 0x7fffffff, 0xff80000000ULL, 0x017fffffffULL, 0xff7fffffffULL, 0x1234567833ULL, 0x80aa001234ULL}[tp];
	u16 const st0 = (0xa604 | (tp & 1 ? 0x600 : 0) | (mp & 1 ? 0x800 : 0)) & (bk ? 0xfe00 : 0xffff);
	u16 const st1 = 0x800 | (flags & 1 ? 0x40 : 0) | (flags & 2 ? 0x200 : 0) | (flags & 8 ? 0x100 : 0);
	s64 product = s64(square ? s16(memory) : s16(t)) * s16(memory) * (flags & 1 ? 2 : 1);
	if (!square && (flags & 7) == 7) product = std::clamp(product, s64(-2147483648LL), s64(2147483647LL));
	s64 const signed_source = BIT(source, 39) ? s64(source) - (s64(1) << 40) : s64(source);
	s64 result = operation >= (square ? 1U : 2U) ? signed_source - product : signed_source + product;
	if (!square && (operation & 1)) result = (result + 32768) & ~s64(65535);
	bool const overflow = result < -2147483648LL || result > 2147483647LL;
	u64 const stored = u64(overflow && (flags & 2) ? result < 0 ? -2147483648LL : 2147483647LL : result) & 0xffffffffffULL;
	mac_smem_fixture f{u16(((square ? 0x3800 : 0x2800) + operation * 0x200) | ((selector & 1) << 8) | (form ? 0xf8 : bk ? 0x19 : 0x81)), memory, t, st0, st1, u16(flags & 4 ? 2 : 0), u16(bk ? 0x19 : form == 2 ? 0x11 : 0x500), u16(st0 | (overflow ? selector & 1 ? 0x200 : 0x400 : 0)), selector & 1 ? 0x246813579aULL : source, selector & 1 ? source : 0x246813579aULL, stored, 5 + unsigned(form != 0)};
	f.base = bk && (variant / 16384) % 2 ? 0x20000 : 0;
	return f;
}

tms320c54x_test_state::poly_fixture tms320c54x_test_state::poly_case(unsigned variant) const
{
	if (profile_is("mac-rpt"))
	{
		unsigned v = variant; poly_fixture f{}; f.dual = true;
		f.form = v % 4; v /= 4; f.operation = v % 4; v /= 4; f.selector = v % 4; v /= 4;
		unsigned const pattern = v % 4; v /= 4; unsigned const flags = v % 16; v /= 16;
		f.iterations = std::array<unsigned,4>{1,2,8,256}[v % 4]; v /= 4;
		f.base = v % 2 ? 0x20000 : 0; v /= 2;
		f.checkpoint = std::array<unsigned,3>{5,6+f.iterations/2,6+f.iterations}[v % 3];
		f.st0 = 0x1030 | (pattern & 1 ? 0x400 : 0) | (pattern & 2 ? 0xa00 : 0);
		f.st1 = 0x800 | (flags & 1 ? 0x40 : 0) | (flags & 2 ? 0x200 : 0) | (flags & 8 ? 0x100 : 0);
		f.pmst = flags & 4 ? 2 : 0; f.t = 0x5555;
		f.xseed = std::array<u16,4>{0x8000,0x7fff,0xffff,0x1234}[pattern];
		f.yseed = std::array<u16,4>{0x8000,1,0xffff,0x7fff}[pattern];
		f.a = std::array<u64,4>{0,0x7fffffff,0xff80000000,0x8000000000}[pattern];
		f.b = std::array<u64,4>{0xffffffffff,0x0180000000,0,0x123456789a}[pattern];
		return f;
	}
	if (profile_is("sqacc-repeat") || profile_is("tmac-repeat"))
	{
		unsigned v = variant; poly_fixture f{}; f.square = true;
		f.tmac = profile_is("tmac-repeat");
		unsigned const operations = f.tmac ? 8 : 4;
		f.form = v % 3; v /= 3; f.operation = v % operations; v /= operations;
		unsigned const pattern = v % 8; v /= 8; unsigned const flags = v % 16; v /= 16;
		f.iterations = std::array<unsigned,4>{1,2,8,256}[v % 4]; v /= 4;
		f.base = v % 2 ? 0x20000 : 0; v /= 2;
		f.checkpoint = std::array<unsigned,4>{5,6,7+f.iterations/2,6+f.iterations}[v % 4];
		f.st0 = 0x1030 | (pattern & 1 ? 0x400 : 0) | (pattern & 2 ? 0x200 : 0) | (pattern & 4 ? 0x800 : 0);
		f.st1 = 0x800 | (flags & 1 ? 0x40 : 0) | (flags & 2 ? 0x100 : 0) | (flags & 4 ? 0x200 : 0) | (flags & 8 ? 0x80 : 0);
		if (f.tmac) f.pmst = flags & 8 ? 2 : 0;
		f.t = std::array<u16,8>{0,1,0x5678,0x8000,0xffff,0x7fff,0x8000,0x1234}[pattern];
		f.a = std::array<u64,8>{0,0x12340000,0x12340000,0x0080000000,0x0180000000,0xff80000000,0xff00000000,0x7fffffffff}[pattern];
		f.b = std::array<u64,8>{0,0x8000,0x10000,0x7fffffff,0xff80000000,0x7fff8000,0xffffffffff,0x7fffffffff}[pattern];
		return f;
	}
	unsigned const form = variant % 3, pattern = (variant / 3) % 8, flags = (variant / 24) % 8, repeat = (variant / 192) % 3;
	return {form, repeat == 2 ? 3U : 1U, std::array<unsigned, 4>{4,6,8,12}[variant / 1152],
		u32((variant / 576) % 2 ? 0x20000 : 0),
		u16(0x1a30 | (pattern & 1 ? 0x0400 : 0)),
		u16(0x800 | (flags & 1 ? 0x40 : 0) | (flags & 2 ? 0x100 : 0) | (flags & 4 ? 0x200 : 0)),
		std::array<u16,8>{0,1,0x5678,0x8000,0xffff,0x7fff,0x8000,0x1234}[pattern],
		std::array<u64,8>{0,0x12340000,0x12340000,0x0080000000,0x0180000000,0xff80000000,0xff00000000,0x7fffffffff}[pattern],
		std::array<u64,8>{0,0x8000,0x10000,0x7fffffff,0xff80000000,0x7fff8000,0xffffffffff,0x7fffffffff}[pattern]};
}

u16 tms320c54x_test_state::poly_memory(unsigned ordinal)
{ return std::array<u16,3>{0x8000,0xffff,0x1234}[ordinal]; }

u16 tms320c54x_test_state::mac_repeat_address(poly_fixture const &f, unsigned ordinal, bool y)
{
	if (y && f.form != 3) return 0x1c00 + (f.form == 1 || f.form == 2 ? ordinal : 0);
	return 0x1800 + (f.form == 2 ? -int(ordinal) : f.form ? int(ordinal) : 0);
}

u16 tms320c54x_test_state::mac_repeat_memory(poly_fixture const &f, unsigned ordinal, bool y)
{
	if (f.form == 3) y = false;
	return (y ? f.yseed : f.xseed) ^ u16(ordinal * (y ? 0x2222 : 0x1111));
}

std::array<u64,3> tms320c54x_test_state::mac_repeat_step(poly_fixture const &f, std::array<u64,3> state, unsigned ordinal)
{
	u64 const source = state[f.selector >> 1]; unsigned const destination = f.selector & 1;
	s64 product = s64(s16(mac_repeat_memory(f,ordinal,false))) * s16(mac_repeat_memory(f,ordinal,true)) * (f.st1 & 0x40 ? 2 : 1);
	if ((f.pmst & 2) && (f.st1 & 0x240) == 0x240) product = std::clamp(product,s64(-2147483648LL),s64(2147483647LL));
	s64 result = (source & 0x8000000000ULL ? s64(source)-0x10000000000LL : s64(source)) + (f.operation & 2 ? -product : product);
	if (f.operation & 1) { s64 const biased = result+32768; result = (biased >= 0 ? biased/65536 : -((-biased+65535)/65536))*65536; }
	if (result < -2147483648LL || result > 2147483647LL)
	{
		state[2] |= destination ? 0x200 : 0x400;
		if (f.st1 & 0x200) result = std::clamp(result,s64(-2147483648LL),s64(2147483647LL));
	}
	state[destination] = u64(result) & 0xffffffffffULL; return state;
}

std::array<u64,3> tms320c54x_test_state::poly_reference(poly_fixture const &f)
{
	u64 a = f.a, b = f.b; u16 st0 = f.st0;
	for (unsigned i = 0; i < f.iterations; ++i)
	{
		s64 high = (a >> 16) & 0x1ffff; if (high & 0x10000) high -= 0x20000;
		s64 old_b = b; if (b & 0x8000000000ULL) old_b -= 0x10000000000LL;
		s64 result = high * s16(f.t) * (BIT(f.st1,6) ? 2 : 1) + old_b + 0x8000;
		result = (result >> 16) * 0x10000;
		if (result < -0x80000000LL || result > 0x7fffffffLL)
		{
			st0 |= 0x400;
			if (BIT(f.st1,9)) result = result < 0 ? -0x80000000LL : 0x7fffffffLL;
		}
		a = u64(result) & 0xffffffffffULL;
		u16 const memory = poly_memory(f.form == 2 ? i : 0);
		s64 const coefficient = BIT(f.st1,8) ? s16(memory) : s64(memory);
		b = u64(coefficient * 0x10000) & 0xffffffffffULL;
	}
	return {a,b,st0};
}

void tms320c54x_test_state::start_poly(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	auto const f = poly_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 64; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	unsigned const repeat = f.square || f.dual ? 1 : (variant / 192) % 3;
	program.write_word(f.base + 0x600, repeat ? 0xec00 | (f.iterations - 1) : 0xf495);
	u16 const mode = f.dual ? 0 : std::array<u16,3>{0,0x82,0x92}[f.form];
	program.write_word(f.base+0x601,f.dual ? 0xb000 | (f.operation << 10) | (f.selector << 8) | std::array<u16,4>{0x01,0x9a,0x49,0x84}[f.form] :
		f.square ? (f.tmac ? 0x2800 : 0x3800) | (f.operation << 8) | mode : 0x3600 | mode);
	program.write_word(f.base + 0x602, 0xf4e1);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE,0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL,0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST,f.pmst); m_cpu->set_state_int(tms320c54x_device::STATE_ST0,f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1,f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_A,f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B,f.b); m_cpu->set_state_int(tms320c54x_device::STATE_T,f.t); m_cpu->set_state_int(tms320c54x_device::STATE_AR2,0x1800);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR,0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR,0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR,0);
	m_cpu->set_input_line(0,CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI,CLEAR_LINE);
	for (unsigned i = 0; i < 3; ++i) data.write_word(0x1800+i,poly_memory(i));
	if (f.square)
	{
		for (unsigned i = 0; i < f.iterations; ++i) data.write_word(0x1800+i, square_repeat_memory(f,i));
		m_repeat_load_expected = {{{f.a, f.b, f.st0}}};
		for (unsigned i = 0; i < f.iterations; ++i) m_repeat_load_expected.push_back(square_repeat_step(f, m_repeat_load_expected.back(), i));
	}
	if (f.dual)
	{
		for (unsigned ar = 2; ar < 6; ++ar) m_cpu->set_state_int(tms320c54x_device::STATE_AR0+ar,0x4800+ar);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR0+(f.form == 1 ? 3 : 2),0x1800);
		if (f.form != 3) m_cpu->set_state_int(tms320c54x_device::STATE_AR0+(f.form == 1 ? 4 : 3),0x1c00);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR0,3); m_cpu->set_state_int(tms320c54x_device::STATE_BK,0);
		for (unsigned i = 0; i < (f.form ? f.iterations : 1U); ++i) for (bool y : {false,true}) data.write_word(mac_repeat_address(f,i,y),mac_repeat_memory(f,i,y));
		m_repeat_load_expected = {{{f.a,f.b,f.st0}}};
		for (unsigned i = 0; i < f.iterations; ++i) m_repeat_load_expected.push_back(mac_repeat_step(f,m_repeat_load_expected.back(),i));
	}
	m_pipeline_operand_reads = m_pipeline_store_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0,0xffff,"poly_source",[this,f](offs_t at,u16 &word,u16)
	{
		if (machine().side_effects_disabled()) return;
		if (f.dual)
		{
			unsigned const ordinal = m_pipeline_operand_reads++, i = ordinal/2; bool const y = ordinal & 1;
			expect(i < f.iterations && at == mac_repeat_address(f,i,y) && word == mac_repeat_memory(f,i,y) && m_cpu->pipeline_cycles() == 6+i, "Repeated dual MAC retains both ordered Read samples on each clock");
			if (y)
			{
				auto const disable = machine().disable_side_effects();
				for (bool side : {false,true}) m_cpu->space(AS_DATA).write_word(mac_repeat_address(f,i,side),f.form ? u16(~mac_repeat_memory(f,i,side)) : mac_repeat_memory(f,i+1,side));
			}
			return;
		}
		if (f.square)
		{
			unsigned const i = m_pipeline_operand_reads++;
			expect(i < f.iterations && at == 0x1800 + (f.form == 2 ? i : 0) && word == square_repeat_memory(f,i), "Repeated square owns each fresh RAM read in order");
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(at, u16(~word)); return;
		}
		unsigned const i = f.form == 2 ? m_pipeline_operand_reads : 0;
		expect(m_pipeline_operand_reads < f.iterations && at == 0x1800+i && word == poly_memory(i),"POLY owns one ordered coefficient read per iteration"); ++m_pipeline_operand_reads;
	});
	m_pipeline_write_tap = data.install_write_tap(0,0xffff,"poly_no_write",[this](offs_t,u16 &,u16) { if (!machine().side_effects_disabled()) { ++m_pipeline_store_writes; expect(false,"POLY must not write data memory"); } });
	m_pipeline_extensions_reads = 0;
	if (f.square || f.dual) m_pipeline_fetch_tap = program.install_read_tap(f.base+0x601,f.base+0x601,"repeated_arithmetic_opcode",[this](offs_t,u16 &,u16) { if (!machine().side_effects_disabled()) expect(!m_pipeline_extensions_reads++, "Repeated arithmetic fetches its body opcode only once"); });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 399980;
	m_cpu->set_state_int(STATE_GENPC,f.base+0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock()));
}

tms320c54x_test_state::register_product_fixture tms320c54x_test_state::register_product_case(unsigned variant)
{
	if (variant >= 176640)
	{
		// Separate domains: POLY excludes SMUL; immediate/absolute products
		// include extension ownership; the final bank isolates EXP producers.
		unsigned v = variant - 176640;
		bool const poly = v < 2304;
		bool const immediate = !poly && v < 29952;
		bool const extended = v >= 29952 && v < 94464;
		bool const exponent = v >= 94464;
		v -= poly ? 0 : immediate ? 2304 : extended ? 29952 : 94464;
		unsigned const operation = v % (poly ? 1 : immediate ? 6 : extended ? 14 : 46); v /= poly ? 1 : immediate ? 6 : extended ? 14 : 46;
		unsigned const pattern = v % 8; v /= 8;
		unsigned flags = v % (poly ? 8 : 16); v /= poly ? 8 : 16;
		if (poly) flags = (flags & 3) | ((flags & 4) << 1); // POLY SMUL-set arithmetic is not validated.
		unsigned const mode = exponent ? 5 : v % 6; if (!exponent) v /= 6;
		unsigned const page = v % 2; v /= 2;
		auto f = register_product_case((exponent && operation < 18 ? operation : 0) + 18 * (pattern + 8 * (flags + 16 * ((mode == 5 ? 0 : mode) + 5 * (page + 2 * v)))));
		f.mode = mode; f.poly = poly; f.immediate = immediate;
		f.length = immediate || extended ? 2 : 1;
		f.checkpoint = std::array<unsigned,3>{4,6+f.length,7+f.length}[v];
		f.smem = extended || (exponent && operation >= 18);
		f.mapped_t = exponent && operation >= 32;
		if (poly) f.opcode = 0x3681;
		else if (immediate) f.opcode = std::array<u16,6>{0xf066,0xf166,0xf067,0xf167,0xf267,0xf367}[operation];
		else if (f.smem) f.opcode = std::array<u16,14>{0x2000,0x2100,0x2200,0x2300,0x2400,0x2500,0x2800,0x2900,0x2a00,0x2b00,0x2c00,0x2d00,0x2e00,0x2f00}[extended ? operation : (operation-18)%14] | (extended ? 0xf8 : f.mapped_t ? 0x0e : 0x81);
		if (f.mapped_t) f.st0 &= ~0x1ff;
		return f;
	}
	if (variant >= 69120)
	{
		unsigned v = variant - 69120, operation = v % 14; v /= 14;
		unsigned const pattern = v % 8; v /= 8; unsigned const flags = v % 16; v /= 16;
		unsigned const mode = v % 5; v /= 5; unsigned const page = v % 2; v /= 2;
		unsigned const snapshot = v % 3; v /= 3;
		auto f = register_product_case(18 * (pattern + 8 * (flags + 16 * (mode + 5 * (page + 2 * snapshot)))));
		f.smem = true; f.mapped_t = v;
		f.opcode = std::array<u16,14>{0x2000,0x2100,0x2200,0x2300,0x2400,0x2500,0x2800,0x2900,0x2a00,0x2b00,0x2c00,0x2d00,0x2e00,0x2f00}[operation] | (f.mapped_t ? 0x0e : 0x81);
		if (f.mapped_t) f.st0 &= ~0x1ff;
		return f;
	}
	unsigned v = variant, operation = v % 18; v /= 18;
	unsigned const pattern = v % 8; v /= 8; unsigned const flags = v % 16; v /= 16;
	unsigned const mode = v % 5; v /= 5; u32 const base = v % 2 ? 0x20000 : 0; v /= 2;
	u16 const opcode = operation < 2 ? 0xf48c | (operation << 8) : 0xf488 | ((operation-2)&3) | (((operation-2)&4)<<6) | (((operation-2)&8)<<6);
	return {opcode, std::array<u16,8>{0,1,0x4567,0x8000,0xffff,0x7fff,0x8001,0x1234}[pattern],
		u16(0x1804 | (pattern&1 ? 0x600 : 0)), u16(0x800 | (flags&1 ? 0x40 : 0) | (flags&2 ? 0x200 : 0) | (flags&8 ? 0x100 : 0)), u16(flags&4 ? 2 : 0),
		std::array<u64,8>{0,0x12340000,0xff87651111,0x0080000000,0x0180000000,0xff80000000,0xff00000000,0x7fffffffff}[pattern],
		std::array<u64,8>{0x1234,0x7fff8000,0x3200000,0xff80000000,0x7fffffff,0xffffffffff,0x80aa001234,0x7fffffffff}[pattern],base,mode,std::array<unsigned,3>{4,7,8}[v]};
}

u16 tms320c54x_test_state::product_input_t(register_product_fixture const &f)
{
	if (f.mode == 1 || f.mode == 2) return u16(f.b);
	if (f.mode == 5) return exponent_fixture{f.a,false,false,0,f.st0,f.st1,f.base}.exponent();
	return f.t;
}

std::array<u64,3> tms320c54x_test_state::register_product_reference(register_product_fixture const &f)
{
	u64 const a = f.mode == 3 ? 1 : f.a;
	u16 const t = product_input_t(f);
	s64 high = (a >> 16) & 0x1ffff; if (high & 0x10000) high -= 0x20000;
	unsigned const family = (f.opcode >> 8) & 0x0e;
	u16 const memory = f.mapped_t ? t : 0x8001;
	s64 product = (f.immediate ? s64(s16(t)) * s16(memory) : f.smem ? family == 4 ? s64(t) * memory : s64(s16(t)) * s16(memory) : high * s16(t)) * (BIT(f.st1,6) ? 2 : 1);
	bool const multiply = f.poly ? false : f.immediate ? (f.opcode & 0xfeff) == 0xf066 : f.smem ? family <= 4 : (f.opcode & 0xfeff) == 0xf48c;
	if (!multiply && (f.pmst&2) && (f.st1&0x240)==0x240) product = std::clamp(product,s64(-0x80000000LL),s64(0x7fffffffLL));
	u64 const source = f.poly || BIT(f.opcode,f.smem ? 8 : 9) ? f.b : a;
	s64 signed_source = source; if (source & 0x8000000000ULL) signed_source -= 0x10000000000LL;
	s64 result = multiply ? product : signed_source + ((!f.poly && !f.immediate && (f.smem ? family >= 12 : BIT(f.opcode,1))) ? -product : product);
	if (f.poly || (!f.immediate && (f.smem ? family == 2 || family == 10 || family == 14 : !multiply && BIT(f.opcode,0)))) result = ((result+0x8000)>>16)*0x10000;
	u16 st0 = f.st0;
	if (result < -0x80000000LL || result > 0x7fffffffLL)
	{
		st0 |= BIT(f.opcode,8) ? 0x200 : 0x400;
		if (BIT(f.st1,9)) result = result < 0 ? -0x80000000LL : 0x7fffffffLL;
	}
	u64 const stored = u64(result) & 0xffffffffffULL;
	if (f.poly) return {stored,u64((BIT(f.st1,8) ? s64(s16(memory)) : s64(memory))*0x10000)&0xffffffffffULL,st0};
	return {BIT(f.opcode,8) ? a : stored, BIT(f.opcode,8) ? stored : f.b, st0};
}

void tms320c54x_test_state::start_register_product(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE,true); m_pipeline_peripheral_case = variant;
	auto const f = register_product_case(variant); auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i=0;i<16;++i) program.write_word(f.base+0x600+i,0xf495);
	if (f.mode == 1 || f.mode == 2) program.write_word(f.base+0x600+(f.mode==1),0x890e);
	if (f.mode == 3) program.write_word(f.base+0x601,0xe801);
	if (f.mode == 5) program.write_word(f.base+0x601,0xf48e);
	program.write_word(f.base+0x602,f.opcode);
	if (f.length == 2) program.write_word(f.base+0x603,f.immediate ? 0x8001 : 0x1800);
	program.write_word(f.base+0x602+f.length,0xf4e1);
	if (f.mode == 4) { program.write_word(f.base+0x602+f.length,0x770e); program.write_word(f.base+0x603+f.length,0xbeef); program.write_word(f.base+0x604+f.length,0xf4e1); }
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE,0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL,0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST,f.pmst); m_cpu->set_state_int(tms320c54x_device::STATE_ST0,f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1,f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_A,f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B,f.b); m_cpu->set_state_int(tms320c54x_device::STATE_T,f.t);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR1,0x1800); data.write_word(0x1800,0x8001);
	m_cpu->set_state_int(tms320c54x_device::STATE_TCR,0x10); m_cpu->set_state_int(tms320c54x_device::STATE_IFR,0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR,0);
	m_cpu->set_input_line(0,CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI,CLEAR_LINE);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = m_pipeline_register_writes = 0;
	m_pipeline_operand_tap = data.install_read_tap(0,0xffff,"product_source",[this,f](offs_t at,u16 &word,u16) {
		if (machine().side_effects_disabled()) return;
		u16 const t = product_input_t(f);
		expect((f.smem || f.poly) && !m_pipeline_operand_reads && at==(f.mapped_t ? 0x0e : 0x1800) && word==(f.mapped_t ? t : 0x8001) && m_cpu->pipeline_cycles()==6+f.length,"Product reads its RAM or zero-latency mapped T source once in Read"); ++m_pipeline_operand_reads;
	});
	m_pipeline_write_tap = data.install_write_tap(0,0xffff,"register_product_t_producer",[this,f](offs_t at,u16 &word,u16)
	{ if (machine().side_effects_disabled()) return; expect((f.mode==1 || f.mode==2 || f.mode==4) && !m_pipeline_store_writes && at==0x0e && word==(f.mode==4 ? 0xbeef : u16(f.b)),"Only the fixture's STLM or following STM may write T"); ++m_pipeline_store_writes; });
	m_pipeline_fetch_tap = program.install_read_tap(f.base+0x602,f.base+0x601+f.length,"product_fetch",[this,f](offs_t at,u16 &word,u16) {
		if (machine().side_effects_disabled()) return;
		if (at==f.base+0x602) { expect(!m_pipeline_extensions_reads && word==f.opcode,"Product owns one opcode fetch"); ++m_pipeline_extensions_reads; }
		else { expect(!m_pipeline_register_writes && word==(f.immediate ? 0x8001 : 0x1800),"Product owns one retained immediate or source-address extension"); ++m_pipeline_register_writes; }
	});
	m_pipeline_replay=false; m_idle_fixture_tick=0; m_phase=400000;
	m_cpu->set_state_int(STATE_GENPC,f.base+0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock()));
}

void tms320c54x_test_state::start_mac_smem(unsigned variant)
{
	m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_pipeline_peripheral_case = variant;
	bool const bk = profile_is("mac-bk");
	bool const square = !bk && variant >= 73728;
	auto const f = mac_smem_case(square ? variant - 73728 : variant, bk, square); bool const absolute = u8(f.opcode) == 0xf8;
	auto &program = m_cpu->space(AS_PROGRAM); auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 32; ++i) program.write_word(f.base + 0x600 + i, 0xf495);
	program.write_word(f.base + 0x600, f.opcode); if (absolute) program.write_word(f.base + 0x601, f.address);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0); m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, f.pmst); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.st0); m_cpu->set_state_int(tms320c54x_device::STATE_ST1, f.st1);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b); m_cpu->set_state_int(tms320c54x_device::STATE_T, f.t); m_cpu->set_state_int(tms320c54x_device::STATE_AR1, 0x500);
	m_cpu->set_state_int(tms320c54x_device::STATE_IFR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_IMR, 0); m_cpu->set_state_int(tms320c54x_device::STATE_TCR, 0x10);
	m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); data.write_word(f.address, f.memory);
	m_pipeline_operand_reads = m_pipeline_store_writes = m_pipeline_extensions_reads = 0;
	m_pipeline_operand_tap = data.install_read_tap(0, 0xffff, "mac_smem_read",
		[this, f](offs_t at, u16 &word, u16) { if (machine().side_effects_disabled()) return; expect(!m_pipeline_operand_reads && at == f.address && word == f.memory && m_cpu->pipeline_cycles() == f.read, "MAC/MAS retain one RAM or mapped-register source in Read"); ++m_pipeline_operand_reads; });
	m_pipeline_write_tap = data.install_write_tap(0, 0xffff, "mac_smem_no_write",
		[this](offs_t, u16 &, u16) { if (!machine().side_effects_disabled()) { ++m_pipeline_store_writes; expect(false, "MAC/MAS must not write data memory or republish T"); } });
	m_pipeline_fetch_tap = program.install_read_tap(f.base + 0x601, f.base + 0x601, "mac_smem_literal",
		[this, f, absolute](offs_t, u16 &word, u16) { if (machine().side_effects_disabled() || !absolute) return; expect(!m_pipeline_extensions_reads && word == f.address, "MAC/MAS own one absolute source extension"); ++m_pipeline_extensions_reads; });
	m_pipeline_replay = false; m_idle_fixture_tick = 0; m_phase = 253690;
	m_cpu->set_state_int(STATE_GENPC, f.base + 0x600); m_cpu->set_pipeline_frontend(true); m_cpu->resume(SUSPEND_REASON_DISABLE); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}




bool tms320c54x_test_state::shifted_unowned_fixture() const
{ return profile_is("shift-unowned"); }
































bool tms320c54x_test_state::check_accumulator_shift_results()
{
	if (m_phase == 252630)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const reverse = profile_is("drsub");
		auto const f = reverse ? long_reverse_fixture(variant) : long_pair_fixture(variant);
		bool const destination = BIT(f.opcode, 8), retired = tick >= f.read_cycle + 1;
		bool const source = (f.opcode & 0xfc00) == 0x5000 ? BIT(f.opcode, 9) : destination;
		u64 const initial_a = source ? 0x246813579aULL : f.source, initial_b = source ? f.source : 0x246813579aULL;
		unsigned const checkpoint = f.read_cycle - 2 + std::array<unsigned, 3>{0, 2, 3}[variant < 6912 ? variant / 2304 : (variant - 6912) / 4608];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (retired && !destination ? f.result : initial_a) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (retired && destination ? f.result : initial_b) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (retired ? f.result_st0 : f.st0) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1,
			"Long arithmetic publishes full/dual-lane result and modeled carry/overflow only at Execute");
		if (tick == f.read_cycle - 2 && f.form >= 5)
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_PROGRAM).write_word((f.reg == 6 ? 0x20000 : 0) + 0x601, 0xdead);
		}
		if (tick == f.read_cycle - 1)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + f.reg) == f.next_ar, "DSUB long addressing modifies AR by its specified offset or two-word step once");
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.reg, 0x2000);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x2000);
		}
		if (tick == f.read_cycle)
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_DATA).write_word(f.address, 0xdead); m_cpu->space(AS_DATA).write_word(f.address ^ 1, 0xbeef);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save DSUB after Decode, paired read or Execute");
		}
		if (tick == 16)
		{
			expect(m_pipeline_operand_reads == 2 && m_pipeline_extensions_reads == (f.form >= 5), "DSUB owns exactly one pair and extension");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1];
				return restore_pipeline_at(252640, "Restore DSUB without duplicate reads or resampling changed operands");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (reverse)
			{
				if (variant < 6911) { start_pipeline_long_pair(variant + 1); return true; }
				osd_printf_info("TMS320C54x staged DRSUB: PASS cases=6144 restore=6144 address_forms=8 destinations=A,B patterns=8 c16=0,1 sxm=0,1 ovm=0,1 parity=0,1 pair_reads=2 memory_writes=0 retained_pair=1 silicon_phase_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated DRSUB complete");
			}
			if (variant == 6911) osd_printf_info("TMS320C54x staged DSUB: PASS cases=6144 address_forms=8 destinations=A,B patterns=8 c16=0,1 sxm=0,1 ovm=0,1 parity=0,1 restore=6144 duplicate_reads=0 memory_writes=0 silicon_phase_claim=0\n");
			if (variant < 20735) { start_pipeline_long_pair(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged DADD: PASS cases=12288 address_forms=8 selectors=4 patterns=8 c16=0,1 sxm=0,1 ovm=0,1 parity=0,1 restore=12288 duplicate_reads=0 memory_writes=0 silicon_phase_claim=0\n");
			if (profile_is("long-pair"))
			{
				osd_printf_info("TMS320C54x staged long-pair controls: PASS dsub=6144 dadd=12288 restore=18432 pair_reads=2 memory_writes=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated long-pair controls complete");
			}
			return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252640)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252630;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}


bool tms320c54x_test_state::check_absolute_xorm_results()
{
	if (m_phase == 252890)
	{
		bool const peripheral = profile_is("rmw-long-periph");
		bool const xorm = profile_is("xorm-absolute");
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick, operation = xorm ? 3 : variant % 3, arithmetic = (variant / (xorm ? 4 : peripheral ? 576 : 300)) % 64;
		bool const compatibility = (variant / (peripheral ? 288 : 19200)) % 2;
		auto const f = xorm ? absolute_xorm_fixture(variant) : peripheral ? peripheral_long_rmw_fixture(variant) : long_accumulator_store_fixture((variant / 3) % 100 + (compatibility ? 6400 : 0));
		u16 const initial_st0 = (f.st0 & ~0x0400) | (BIT(arithmetic, 2) << 10) | 0x0800;
		u16 const modified_st0 = compatibility && f.reg && f.kind != 3 ? (initial_st0 & 0x1fff) | (f.reg << 13) : initial_st0;
		auto const expected = long_rmw_expected(arithmetic, operation, modified_st0);
		unsigned const checkpoint = xorm ? std::array<unsigned, 4>{4, 6, 7, 8}[variant / 1024] : peripheral ? peripheral_rmw_schedule::checkpoints[peripheral_rmw_schedule::checkpoint_index(variant)] : long_rmw_schedule::checkpoints[long_rmw_schedule::checkpoint_index(variant)];
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Long-offset RMW has three-word staged ownership");
		if (tick == 5)
		{
			auto const disable = machine().disable_side_effects();
			u32 const base = (peripheral ? bool((variant / 36864) % 2) : f.reg == 6) ? 0x20000 : 0;
			m_cpu->space(AS_PROGRAM).write_word(base + 0x601, 0xdead); m_cpu->space(AS_PROGRAM).write_word(base + 0x602, 0xdead);
		}
		if (tick == 6)
		{
			for (unsigned ar = 0; ar < 8; ++ar)
				expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + ar) == (ar == f.selected ? f.next_ar : peripheral ? 0x5555 : 0x1007),
					"Long RMW modifies only its selected source pointer once at address capture");
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.selected, xorm ? f.address : 0x2222);
		}
		if (tick == (f.address >= 0x20 && f.address < 0x60 ? 8 : 7))
		{
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.address, 0x4444);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save long RMW during Decode, address capture, sampled Read or publication");
		}
		if (tick == 20)
		{
			{
				auto const disable = machine().disable_side_effects();
				expect(m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 && m_pipeline_extensions_reads == 2 &&
					m_cpu->space(AS_DATA).read_word(f.address) == expected[0] && m_cpu->state_int(tms320c54x_device::STATE_ST0) == expected[1] &&
					m_cpu->state_int(tms320c54x_device::STATE_ST1) == (f.st1 | (BIT(arithmetic, 0) << 8) | (BIT(arithmetic, 1) << 9)) &&
					m_cpu->state_int(tms320c54x_device::STATE_AR0 + f.selected) == (xorm ? f.address : 0x2222) &&
					m_cpu->state_int(tms320c54x_device::STATE_A) == (xorm ? (u64(BIT(arithmetic, 0) ? s64(s16(expected[0])) : s64(expected[0])) & 0xffffffffffULL) : 0x123456789aULL) && m_cpu->state_int(tms320c54x_device::STATE_B) == 0x2233445566ULL,
					"Long RMW preserves accumulators, operation-specific status and the captured source despite later pointer/RAM changes");
			}
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2];
				expect(restore_snapshot(machine(), *m_check_timer, m_saved_pipeline) == STATERR_NONE, "Restore long RMW without duplicate reads, writes or pointer updates");
				if (!xorm) ++m_long_rmw_replays;
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 252900; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (!xorm) m_long_rmw_coverage.add(peripheral ? peripheral_rmw_schedule::case_key(variant) : long_rmw_schedule::case_key(variant));
			unsigned const limit = xorm ? 4096 : peripheral ? peripheral_rmw_schedule::unique_cases : long_rmw_schedule::unique_cases;
			if (variant + 1 < limit) { start_pipeline_long_rmw(variant + 1); return true; }
			if (xorm)
			{
				osd_printf_info("TMS320C54x staged absolute XORM: PASS cases=4096 restore=4096 addresses=4 value_status_pairs=64 compatibility=2 pages=2 snapshots=4 reads=1 writes=1 literal_fetches=2 flags_preserved=1 silicon_phase_claim=0\n");
				throw emu_fatalerror(0, "TMS320C54x isolated absolute XORM complete");
			}
			if (peripheral)
			{
				expect(m_long_rmw_coverage.count == limit && m_long_rmw_replays == limit,
					"Every scheduled peripheral RMW case executes and replays exactly once");
				osd_printf_info("TMS320C54x staged long peripheral RMW: PASS cases=%u restore=%u operations=ADDM,ANDM,ORM registers=8 forms=offset,preincrement offsets=3b,39,49 spans=peripheral,RAM value_pairs=8 status_states=8 compatibility=2 pages=2 snapshots=5 reads=1 writes=1 literal_fetches=2 peripheral_extra=1 ram_extra=0 double_charge=0 device_semantics=unvalidated silicon_phase_claim=0 checkpoint_policy=balanced coverage_fnv64=%016llx\n",
					m_long_rmw_coverage.count, m_long_rmw_replays, (unsigned long long)m_long_rmw_coverage.value);
				throw emu_fatalerror(0, "TMS320C54x isolated long peripheral RMW complete");
			}
			expect(m_long_rmw_coverage.count == limit && m_long_rmw_replays == limit,
				"Every scheduled long RMW case executes and replays exactly once");
			osd_printf_info("TMS320C54x staged long RMW: PASS cases=%u operations=ADDM,ANDM,ORM forms=25 offsets=4 value_pairs=8 status_states=8 compatibility=2 restore=%u duplicate_reads=0 duplicate_writes=0 snapshots=4 checkpoint_policy=balanced coverage_fnv64=%016llx\n",
				m_long_rmw_coverage.count, m_long_rmw_replays,
				(unsigned long long)m_long_rmw_coverage.value);
			if (profile_is("rmw-long")) throw emu_fatalerror(3, "TMS320C54x isolated long RAM RMW complete");
			if (profile_is("long-mvkd")) { start_pipeline_long_mvkd(); return true; }
			return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 252900)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 252890;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}


bool tms320c54x_test_state::check_immediate_arithmetic_results()
{
	if (m_phase == 253110)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		auto const f = immediate_arithmetic_case(variant);
		unsigned const checkpoint = f.producer ? std::array<unsigned, 3>{5 + f.gap, 6 + f.gap, 8 + f.gap}[(variant - 27648) / 48] :
			std::array<unsigned, 3>{3, 4, 6}[variant / 9216];
		u64 const original_a = f.producer || (f.selector & 2) ? 0x1111222233ULL : f.source;
		u64 const original_b = f.producer || !(f.selector & 2) ? 0x4444555566ULL : f.source;
		u64 const initial_a = f.producer && tick >= 7 && !(f.selector & 2) ? f.source : original_a;
		u64 const initial_b = f.producer && tick >= 7 && (f.selector & 2) ? f.source : original_b;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (tick >= f.execute_cycle && !(f.selector & 1) ? f.result : initial_a) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (tick >= f.execute_cycle && (f.selector & 1) ? f.result : initial_b) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (tick >= f.execute_cycle ? f.result_st0 : f.st0) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1,
			"Immediate SUB and shift-16 ADD retire once with 40-bit source, SXM, carry, sticky destination overflow and OVM semantics");
		if (tick == 4 || (f.producer && tick == 6 + f.gap))
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, 0xdead);
			if (f.producer && tick == 6 + f.gap) m_cpu->space(AS_PROGRAM).write_word(f.base + 0x603 + f.gap, 0xbeef);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save immediate arithmetic in partial/complete Decode or Read");
		}
		if (tick == 20)
		{
			expect(m_pipeline_extensions_reads == (f.producer ? 2U : 1U), "Immediate arithmetic consumes each source literal once");
			if (f.producer) expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE), "The load-to-SUB producer fixture drains to IDLE");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_extensions_reads = m_recurring_irq_snapshot[0];
				return restore_pipeline_at(253120, "Restore immediate arithmetic without literal refetch or premature publication");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			if (variant < 27791) { start_pipeline_immediate_arithmetic(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged immediate arithmetic: PASS sub_cases=26112 add16_cases=1536 producer_cases=144 selectors=4 shifts=0..16 patterns=8 status_states=8 pages=2 restore=27792 duplicate_literals=0 data_accesses=0 silicon_phase_claim=0\n");
			start_pipeline_accumulator_arithmetic(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253120)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253110;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253130)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const load = variant >= 196896;
		unsigned const model_variant = load ? variant - 196896 : variant;
		auto const f = load ? register_load_case(model_variant) : register_arithmetic_case_full(variant);
		unsigned const checkpoint = (f.producer ? 5 + f.gap : 3) + register_bank_schedule::checkpoint_index(variant);
		auto const expected = tick >= f.execute_cycle ? f.after : f.producer && tick >= 7 ? f.before : f.initial;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == expected[0] && m_cpu->state_int(tms320c54x_device::STATE_B) == expected[1] &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (tick >= f.execute_cycle ? f.result_st0 : f.st0) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1,
			"Register ALU uses original source/destination and signed 40-bit shifts, publishing the operation's status/result only in Execute");
		if (f.producer && tick == 4)
		{
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, 0xdead);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_extensions_reads, m_pipeline_operand_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save register arithmetic in Decode, Access or Read");
		}
		if (tick == 18)
		{
			expect_opcode(f.opcode, m_pipeline_extensions_reads == 1 && m_pipeline_operand_reads == unsigned(f.producer) &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == expected[0] &&
				m_cpu->state_int(tms320c54x_device::STATE_B) == expected[1] &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.result_st0,
				"Register arithmetic retains its opcode and independently checked result/status");
			if (f.producer) expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE), "Load-to-register-arithmetic producer fixture drains to IDLE");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick;
				m_pipeline_extensions_reads = m_recurring_irq_snapshot[0]; m_pipeline_operand_reads = m_recurring_irq_snapshot[1];
				expect(restore_snapshot(machine(), *m_check_timer, m_saved_pipeline) == STATERR_NONE, "Restore register arithmetic without opcode refetch or premature publication");
				++m_register_bank_replays;
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 253140; m_check_timer->enable(true); return true;
			}
			m_register_bank_coverage.add(register_bank_schedule::case_key(variant));
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			if (variant == 196895)
			{
				expect(m_register_bank_coverage.count == register_bank_schedule::arithmetic_cases && m_register_bank_replays == register_bank_schedule::arithmetic_cases,
					"Register arithmetic counts completed standalone and producer restores");
				osd_printf_info("TMS320C54x staged register arithmetic: PASS cases=65536 producer_cases=288 operations=ADD,SUB selectors=4 shifts=-16..15 shift_sources=opcode,ASM patterns=8 status_states=8 pages=2 restore=65824 duplicate_opcodes=0 data_accesses=0 silicon_phase_claim=0 checkpoints=3 checkpoint_policy=balanced producer_policy=exhaustive\n");
			}
			if (auto const next = register_bank_schedule::next(variant); next < register_bank_schedule::end)
				{ start_pipeline_accumulator_arithmetic(next); return true; }
			expect(m_register_bank_coverage.count == register_bank_schedule::cases && m_register_bank_replays == register_bank_schedule::cases,
				"Register-bank digest includes both banks and every retained producer checkpoint");
			osd_printf_info("TMS320C54x staged register LD: PASS cases=65536 producer_cases=144 selectors=4 shifts=-16..15 shift_sources=opcode,ASM patterns=8 status_states=16 pages=2 restore=65680 duplicate_opcodes=0 data_accesses=0 carry_preserved=1 silicon_phase_claim=0 checkpoints=3 checkpoint_policy=balanced producer_policy=exhaustive coverage_cases=%u coverage_restores=%u coverage_fnv64=%016llx\n",
				m_register_bank_coverage.count, m_register_bank_replays, (unsigned long long)m_register_bank_coverage.value);
			if (profile_is("reg-banks")) throw emu_fatalerror(3, "TMS320C54x isolated register banks complete");
			start_pipeline_memory_multiply(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253140)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253130;
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253150)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const square = profile_is("squr-smem");
		auto const f = memory_multiply_case(variant, square); auto const &o = f.operand;
		bool const load_t = variant >= 165888;
		bool const keeps_t = variant >= 55296 && !load_t && !square;
		unsigned const v = variant % 55296;
		unsigned const index = v < 30720 ? v / 10240 : (v - 30720) / 8192;
		unsigned const checkpoint = std::array<unsigned, 3>{3, o.read_cycle - 1, o.read_cycle}[index];
		bool const destination_b = (!keeps_t && !square) || BIT(o.opcode, 8);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_A) == (!load_t && !destination_b && tick > o.read_cycle ? f.result : f.a) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (!load_t && destination_b && tick > o.read_cycle ? f.result : f.b) &&
			m_cpu->state_int(tms320c54x_device::STATE_T) == (tick < o.read_cycle ? f.t : tick == o.read_cycle ? keeps_t ? f.t : o.memory : 0xbeef) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (tick > o.read_cycle ? f.result_st0 : o.st0) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == o.st1,
			"Memory multiply owns T in Read and B/OVB in Execute, without altering A/carry or overwriting a newer T");
		if (o.form >= 14 && tick == 4) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word((o.reg == 6 ? 0x20000 : 0) + 0x601, 0xdead); }
		if (tick == o.read_cycle - 1 && o.address >= 0x60)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + o.selected) == o.next_ar, "Memory multiply modifies its pointer once in Access");
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + o.selected, 0x2222); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x2222);
		}
		if (tick == o.read_cycle)
		{
			if (o.address >= 0x60) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(o.address, 0xdead); }
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xbeef);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save memory multiply in Decode, Access or Read");
		}
		if (tick == 18)
		{
			expect_opcode(o.opcode, m_pipeline_operand_reads == 1 && m_pipeline_extensions_reads == unsigned(o.form >= 14) &&
				m_cpu->state_int(tms320c54x_device::STATE_T) == 0xbeef && m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.result_st0,
				"Memory multiply/T load has no resampling and preserves its independently checked status and later T");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1];
				return restore_pipeline_at(253160, "Restore memory multiply without re-reading or re-publishing T");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (square)
			{
				if (variant < 55295) { start_pipeline_memory_multiply(variant + 1); return true; }
				osd_printf_info("TMS320C54x memory SQUR: PASS cases=52224 restore=52224 forms=RAM,mapped accumulators=2 patterns=8 flags=16 t_publication=Read operand_retained=1 newer_t_preserved=1 duplicate_reads=0 data_writes=0 physical_phase_claim=0\n");
				throw emu_fatalerror(3, "TMS320C54x isolated memory SQUR complete");
			}
			if (variant == 55295) osd_printf_info("TMS320C54x staged memory multiply: PASS ram_cases=27648 mapped_cases=24576 operations=MPYA,MASA,MACA,MACAR patterns=8 flag_states=16 restore=52224 t_read_owned=1 duplicate_reads=0 data_writes=0 silicon_phase_claim=0\n");
			if (variant == 110591) osd_printf_info("TMS320C54x staged MPYU: PASS ram_cases=27648 mapped_cases=24576 accumulators=2 t_values=2 patterns=8 flag_states=16 restore=52224 t_retained=1 duplicate_reads=0 data_writes=0 silicon_phase_claim=0\n");
			if (variant == 165887) osd_printf_info("TMS320C54x staged MPY and MPYR: PASS cases=52224 accumulators=2 rounding=0,1 t_retained=1 restore=52224 duplicate_reads=0 data_writes=0 silicon_phase_claim=0\n");
			if (variant < 221183) { start_pipeline_memory_multiply(variant + 1); return true; }
			osd_printf_info("TMS320C54x staged memory LD T: PASS cases=52224 ram_and_mapped=1 restore=52224 read_publication=1 retirement_rewrite=0 duplicate_reads=0 data_writes=0 silicon_phase_claim=0\n");
			if (profile_is("mul-t-sources")) throw emu_fatalerror(0, "TMS320C54x isolated multiply and T sources complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253160)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253150; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253210)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const bk = profile_is("shifted-bk");
		auto const f = shifted_arithmetic_case(variant, bk); auto const &o = f.operand;
		bool const subc = variant >= 245760;
		bool const destination = BIT(f.extension, 8), retired = tick > o.read_cycle;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == (retired ? destination ? f.a : f.result : 0x1122) &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == (retired ? destination ? f.result : f.b : 0x3344) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST0) == (retired ? f.result_st0 : o.st0) &&
			m_cpu->state_int(tms320c54x_device::STATE_ST1) == o.st1 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x9abc,
			"Shifted ADD/SUB publishes only selected accumulator and arithmetic flags in Execute");
		if (tick == 4 && (!subc || o.form >= 14))
		{
			auto const disable = machine().disable_side_effects();
			m_cpu->space(AS_PROGRAM).write_word((o.reg == 6 ? 0x20000 : 0) + 0x601, 0xdead);
			if (!subc && o.form >= 14) m_cpu->space(AS_PROGRAM).write_word((o.reg == 6 ? 0x20000 : 0) + 0x602, 0xdead);
		}
		if (tick == o.read_cycle - 1 && o.address >= 0x60)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + o.selected) == o.next_ar, "Shifted arithmetic modifies its selected pointer once in Access");
			m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + o.selected, 0x2222); m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x2222);
		}
		if (tick == o.read_cycle)
		{
			auto const disable = machine().disable_side_effects();
			if (o.address >= 0x60) m_cpu->space(AS_DATA).write_word(o.address, 0xdead);
			else m_cpu->set_state_int(bk ? tms320c54x_device::STATE_BK : tms320c54x_device::STATE_AR0 + o.address - 0x10, 0xdead);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.b);
		}
		unsigned const checkpoint_index = shifted_memory_schedule::checkpoint_index(variant, bk);
		unsigned const checkpoint = std::array<unsigned, 3>{!subc && o.form >= 14 ? 4U : 3U, o.read_cycle - 1, o.read_cycle}[checkpoint_index];
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_idle_fixture_saved_tick = tick; m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save shifted arithmetic through Decode, Access or Read");
		}
		if (tick == 18)
		{
			expect(m_pipeline_operand_reads == 1 && m_pipeline_extensions_reads == unsigned(!subc) + unsigned(o.form >= 14), "Memory arithmetic neither resamples nor repeats extension fetches");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = m_idle_fixture_saved_tick; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1];
				expect(restore_snapshot(machine(), *m_check_timer, m_saved_pipeline) == STATERR_NONE, "Restore shifted arithmetic without duplicating sampling or publication");
				++m_shifted_replays;
				m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 253220; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			m_shifted_coverage.add(shifted_memory_schedule::case_key(variant, bk));
			if (bk)
			{
				if (auto const next = shifted_memory_schedule::next(variant, true); next < shifted_memory_schedule::bk_end)
				{ start_pipeline_shifted_arithmetic(next); return true; }
				expect(m_shifted_coverage.count == shifted_memory_schedule::bk_cases && m_shifted_replays == shifted_memory_schedule::bk_cases,
					"Every BK arithmetic stimulus and balanced replay completed once");
				osd_printf_info("TMS320C54x shifted BK arithmetic: PASS cases=%u restore=%u selectors=8 shifts=32 sxm_ovm=4 values=4 pages=2 snapshots=balanced source_sample=Read post_read_change=1 extension_poison=1 writes=0 physical_phase_claim=0 coverage_fnv64=%016llx\n",
					m_shifted_coverage.count, m_shifted_replays, (unsigned long long)m_shifted_coverage.value);
				throw emu_fatalerror(3, "TMS320C54x isolated shifted BK arithmetic complete");
			}
			if (variant + 1 == 147456 + shifted_memory_schedule::mapped_cases)
			{
				expect(m_shifted_coverage.count == shifted_memory_schedule::arithmetic_cases && m_shifted_replays == shifted_memory_schedule::arithmetic_cases,
					"Every shifted ADD/SUB stimulus and balanced replay completed once");
				osd_printf_info("TMS320C54x staged shifted memory ADD/SUB: PASS ram_cases=49152 mapped_ar_cases=32768 selectors=8 shifts=32 sxm_ovm=4 patterns=4 restore=81920 snapshots=balanced duplicate_reads=0 data_writes=0 silicon_phase_claim=0\n");
			}
			if (auto const next = shifted_memory_schedule::next(variant, false); next < shifted_memory_schedule::end)
			{ start_pipeline_shifted_arithmetic(next); return true; }
			expect(m_shifted_coverage.count == shifted_memory_schedule::cases && m_shifted_replays == shifted_memory_schedule::cases,
				"Every shifted-memory and SUBC stimulus and balanced replay completed once");
			osd_printf_info("TMS320C54x staged SUBC: PASS ram_cases=6144 mapped_ar_cases=4096 value_pairs=64 sxm_ovm=4 accumulators=2 restore=10240 snapshots=balanced duplicate_reads=0 data_writes=0 silicon_phase_claim=0 combined_cases=%u combined_restore=%u coverage_fnv64=%016llx\n",
				m_shifted_coverage.count, m_shifted_replays, (unsigned long long)m_shifted_coverage.value);
			if (profile_is("shifted-alu")) throw emu_fatalerror(3, "TMS320C54x isolated shifted memory arithmetic complete");
			start_pipeline_memory_repeat(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253220)
	{
		m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253210; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_rptbd_shift_store_results()
{
	if (m_phase == 310400)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		auto const f = rptbd_shift_store_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Complete shifted-store delay belongs to RPTBD, not its repeated body");
		if (tick == 3 || tick == 5) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + (tick == 3 ? 0x601 : 0x603), 0xdead); }
		if (tick == 8) m_cpu->set_state_int(f.kind & 1 ? tms320c54x_device::STATE_B : tms320c54x_device::STATE_A, f.source);
		if (f.irq && tick == (f.irq <= 2 ? 5U : 10U)) m_cpu->set_input_line(f.irq == 2 || f.irq == 4 ? INPUT_LINE_NMI : 2, ASSERT_LINE);
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_rptbd_saved_counts = m_rptbd_counts; m_rptbd_saved_return = m_rptbd_return; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save RPTBD delay decode, Read or body/interrupt continuation");
		}
		if (tick == 96)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + 0x610 &&
				m_cpu->state_int(tms320c54x_device::STATE_AR3) == f.final_ar3 && m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x6000 + f.count + 1 &&
				m_cpu->state_int(tms320c54x_device::STATE_A) == (f.kind & 1 ? 0x123456789aULL : 0) && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.kind & 1 ? 0 : 0xfedcba9876ULL) &&
				m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0 && m_cpu->state_int(tms320c54x_device::STATE_RSA) == 0x604 && m_cpu->state_int(tms320c54x_device::STATE_REA) == 0x605 &&
				m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.st0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 &&
				m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000 && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5aa5 && m_cpu->state_int(tms320c54x_device::STATE_BK) == 13,
				"RPTBD executes one shifted delay write and pointer effect, then clears its source and executes the body BRC+1 times");
			expect(m_rptbd_counts.fetches == 3 && m_rptbd_counts.delay_writes == 1 && m_rptbd_counts.acks == unsigned(bool(f.irq)) &&
				m_rptbd_counts.stack_reads == unsigned(bool(f.irq)) && m_rptbd_counts.stack_writes == unsigned(bool(f.irq)), "RPTBD retains words and balances its one optional interrupt across replay");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_rptbd_counts = m_rptbd_saved_counts; m_rptbd_return = m_rptbd_saved_return;
				return restore_pipeline_at(310410, "Restore complete shifted delay without duplicate publication or pointer work");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < 115199) { start_rptbd_shift_store(variant + 1); return true; }
			osd_printf_info("TMS320C54x RPTBD shifted delay stores: PASS cases=115200 restore=115200 counts=0,1,2,7 forms=direct,plain,decrement,postincrement,mapped_encoding selectors=4 shifts=-16,-1,15 sst_sxm=4 patterns=2 pages=2 cpl=2 sources=none,IRQ,NMI arrivals=delay,body snapshots=3 delay_writes=1 pointer_effect=once body_count=brc+1 execute_live_source=1 retained_words=1 balanced_frames=1 physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated RPTBD shifted stores complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 310410) { m_cpu->set_input_line(2, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); m_phase = 310411; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	if (m_phase == 310411) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 310400; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}







bool tms320c54x_test_state::check_poly_results()
{
	if (m_phase == 399980)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = poly_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),"POLY is admitted by the staged frontend");
		if (f.dual)
		{
			unsigned const completed = tick < 7 ? 0 : std::min(tick-6,f.iterations); auto const expected = m_repeat_load_expected[completed];
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == expected[0] && m_cpu->state_int(tms320c54x_device::STATE_B) == expected[1] && m_cpu->state_int(tms320c54x_device::STATE_ST0) == expected[2], "Repeated dual MAC uses retained paired operands and the Execute-live feedback accumulator");
			u16 const t = tick < 6 ? f.t : tick > f.iterations+5 ? 0xdead : mac_repeat_memory(f,tick-6,false);
			expect(m_cpu->state_int(tms320c54x_device::STATE_T) == t, "Repeated dual MAC publishes X in Read without re-publishing T at Execute");
			if (tick == f.iterations+5) m_cpu->set_state_int(tms320c54x_device::STATE_T,0xdead);
			if (tick == 4) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base+0x601,0xe8ff); }
		}
		if (f.square)
		{
			unsigned const completed = tick < 7 ? 0 : std::min(tick-6,f.iterations);
			auto const expected = m_repeat_load_expected[completed];
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == expected[0] && m_cpu->state_int(tms320c54x_device::STATE_B) == expected[1] && m_cpu->state_int(tms320c54x_device::STATE_ST0) == expected[2], "Every repeated square samples the live accumulator at Execute and preserves carry");
			u16 const t = tick > f.iterations+5 ? 0xdead : f.tmac || tick < 6 ? f.t : square_repeat_memory(f,tick-6);
			expect(m_cpu->state_int(tms320c54x_device::STATE_T) == t, "Repeated square publishes each original source at Read without overwriting newer T at Execute");
			if (tick == f.iterations+5) m_cpu->set_state_int(tms320c54x_device::STATE_T,0xdead);
			if (tick == 4) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base+0x601,0xe8ff); }
		}
		if (tick == f.checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads,m_pipeline_store_writes,m_pipeline_extensions_reads};
			expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE,"Save POLY operand and repeated arithmetic progress");
		}
		if (tick == (f.square || f.dual ? f.iterations+24 : 48))
		{
			auto const expected = f.square || f.dual ? m_repeat_load_expected.back() : poly_reference(f);
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == expected[0] && m_cpu->state_int(tms320c54x_device::STATE_B) == expected[1] && m_cpu->state_int(tms320c54x_device::STATE_ST0) == expected[2],"POLY uses signed 17-bit A high, old B, rounding, saturation and coefficient extension");
			expect(m_cpu->state_int(tms320c54x_device::STATE_T) == (f.square || f.dual ? 0xdead : f.t) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && (f.dual || m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x1800+(f.form == 2 ? f.iterations : 0)),"Repeated arithmetic preserves newer T/status and updates its source pointer once per iteration");
			if (f.square || f.dual) expect(m_pipeline_extensions_reads == 1 && m_cpu->state_int(STATE_GENPC) == f.base+0x603, "Repeated arithmetic retains one opcode and reaches its fresh continuation");
			if (f.dual)
			{
				for (unsigned ar = 2; ar < 6; ++ar)
				{
					u16 const address = ar == (f.form == 1 ? 3U : 2U) ? mac_repeat_address(f,f.iterations,false) : f.form != 3 && ar == (f.form == 1 ? 4U : 3U) ? mac_repeat_address(f,f.iterations,true) : 0x4800+ar;
					expect(m_cpu->state_int(tms320c54x_device::STATE_AR0+ar) == address, "Repeated dual MAC modifies each selected pointer once, with X priority on aliases");
				}
				expect(m_cpu->state_int(tms320c54x_device::STATE_PMST) == f.pmst, "Repeated dual MAC preserves product-saturation mode");
			}
			expect(m_pipeline_operand_reads == f.iterations*(f.dual ? 2 : 1) && !m_pipeline_store_writes && m_cpu->state_int(tms320c54x_device::STATE_IDLE),"Repeated arithmetic completes exact reads and reaches the following IDLE");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[2];
				if (f.tmac) m_cpu->set_pipeline_frontend(true);
				return restore_pipeline_at(399990, "Replay POLY without duplicate coefficient reads");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (f.dual || f.tmac ? 98303 : f.square ? 49151 : 4607)) { start_poly(variant+1); return true; }
			if (f.tmac) { osd_printf_info("TMS320C54x repeated T MAC: PASS cases=98304 restore=98304 operations=MAC,MACR,MAS,MASR destinations=A,B forms=DP,indirect,postincrement patterns=8 flags=16 counts=0,1,7,255 pages=2 snapshots=4 per_clock_accumulator=1 t_preserved=1 source_opcode_poison=1 data_writes=0 physical_phase_claim=0\n"); throw emu_fatalerror(3,"TMS320C54x isolated repeated T MAC complete"); }
			if (f.dual) { osd_printf_info("TMS320C54x repeated dual MAC: PASS cases=98304 restore=98304 operations=MAC,MACR,MAS,MASR selectors=4 layouts=fixed,postinc,postdec,alias patterns=4 flags=16 counts=0,1,7,255 pages=2 snapshots=3 per_clock_accumulator=1 paired_reads=1 t_publish=Read newer_t_preserved=1 source_opcode_poison=1 writes=0 physical_banks=unvalidated physical_phase_claim=0\n"); throw emu_fatalerror(3,"TMS320C54x isolated repeated dual MAC complete"); }
			if (f.square) { osd_printf_info("TMS320C54x repeated square accumulate: PASS cases=49152 restore=49152 operations=SQURA,SQURS destinations=A,B forms=DP,indirect,postincrement patterns=8 flags=16 counts=0,1,7,255 pages=2 snapshots=4 per_clock_accumulator=1 t_publish=Read newer_t_preserved=1 source_opcode_poison=1 data_writes=0 physical_phase_claim=0\n"); throw emu_fatalerror(3,"TMS320C54x isolated repeated square accumulate complete"); }
			osd_printf_info("TMS320C54x staged POLY: PASS cases=4608 restore=4608 forms=direct,indirect,postincrement patterns=8 flags=8 repeat=none,0,2 pages=2 snapshots=4 coefficient_reads=iterations data_writes=0 mapped_acceptance=0 extended_acceptance=0 irq_acceptance=0 physical_phase_claim=0\n");
			start_register_product(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true;
	}
	if (m_phase == 399990) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 399980; m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true; }
	if (m_phase == 400000)
	{
		unsigned const variant=m_pipeline_peripheral_case, tick=++m_idle_fixture_tick; auto const f=register_product_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL),"T-input products are admitted by the staged frontend");
		if (tick==6+f.length)
		{
			m_cpu->set_state_int(tms320c54x_device::STATE_T,0xdead);
			auto const disable=machine().disable_side_effects();
			if ((f.smem || f.poly) && !f.mapped_t) m_cpu->space(AS_DATA).write_word(0x1800,0xdead);
			if (f.length==2) m_cpu->space(AS_PROGRAM).write_word(f.base+0x603,0xdead);
		}
		if (tick==f.checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot={m_pipeline_operand_reads,m_pipeline_store_writes,m_pipeline_extensions_reads,m_pipeline_register_writes}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE,"Save T-input products before/after operand capture and Execute");
		}
		if (tick==24)
		{
			auto const result=register_product_reference(f);
			if (m_cpu->state_int(tms320c54x_device::STATE_A)!=result[0] || m_cpu->state_int(tms320c54x_device::STATE_B)!=result[1] || m_cpu->state_int(tms320c54x_device::STATE_ST0)!=result[2])
				osd_printf_info("product_dependency_failure: case=%u opcode=%04x layout=%u replay=%u a=%010llx b=%010llx expected_a=%010llx expected_b=%010llx\n",variant,f.opcode,f.mode,unsigned(m_pipeline_replay),(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B),(unsigned long long)result[0],(unsigned long long)result[1]);
			expect_opcode(f.opcode,m_cpu->state_int(tms320c54x_device::STATE_A)==result[0] && m_cpu->state_int(tms320c54x_device::STATE_B)==result[1] && m_cpu->state_int(tms320c54x_device::STATE_ST0)==result[2],"T-input products use retained operands and Execute accumulator sources with correct arithmetic and flags");
			expect(m_cpu->state_int(tms320c54x_device::STATE_T)==(f.mode==4 ? 0xbeef : 0xdead) && m_cpu->state_int(tms320c54x_device::STATE_ST1)==f.st1,"T-input products preserve later T and ST1");
			expect(m_pipeline_operand_reads==unsigned(f.smem || f.poly) && m_pipeline_store_writes==unsigned(f.mode==1 || f.mode==2 || f.mode==4) && m_pipeline_extensions_reads==1 && m_pipeline_register_writes==f.length-1 && m_cpu->state_int(tms320c54x_device::STATE_IDLE),"Products own only their source, extension and producer transactions and reach the following IDLE");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay=true; m_idle_fixture_tick=f.checkpoint; m_pipeline_operand_reads=m_recurring_irq_snapshot[0]; m_pipeline_store_writes=m_recurring_irq_snapshot[1]; m_pipeline_extensions_reads=m_recurring_irq_snapshot[2]; m_pipeline_register_writes=m_recurring_irq_snapshot[3];
				return restore_pipeline_at(400010, "Replay T-input product operand ownership");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant<69119) { start_register_product(variant+1); return true; }
			if (variant>=176640)
			{
				if (variant<306431) { start_register_product(variant+1); return true; }
				osd_printf_info("TMS320C54x staged remaining T dependencies: PASS cases=129792 restore=129792 POLY=2304 immediate=27648 absolute_Smem=64512 EXP_products=35328 patterns=8 layouts=6 pages=2 snapshots=3 poly_smul=excluded source_reads=0,1 literal_fetches=0,1 t_sample=Read repeat_acceptance=0 irq_acceptance=0 physical_phase_claim=0\n");
				throw emu_fatalerror(0,"TMS320C54x isolated MAC/MAS complete");
			}
			if (variant>=69120)
			{
				if (variant<176639) { start_register_product(variant+1); return true; }
				osd_printf_info("TMS320C54x staged Smem T dependencies: PASS cases=107520 restore=107520 opcodes=14 patterns=8 flags=16 layouts=5 sources=RAM,mapped_T pages=2 snapshots=3 source_reads=1 t_sample=Read repeat_acceptance=0 irq_acceptance=0 physical_phase_claim=0\n");
				start_register_product(176640); return true;
			}
			osd_printf_info("TMS320C54x staged accumulator T products: PASS cases=69120 restore=69120 opcodes=18 patterns=8 flags=16 layouts=none,STLM_zero_gap,STLM_one_gap,A_producer,T_successor pages=2 snapshots=3 t_sample=Read accumulator_sample=Execute overflow_initial=clear,sticky data_reads=0 producer_writes=0,1 opcode_fetches=1 repeat_acceptance=0 irq_acceptance=0 physical_phase_claim=0\n");
			start_register_product(69120); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true;
	}
	if (m_phase==400010) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase=400000; m_check_timer->adjust(attotime::from_ticks(1,m_cpu->clock())); return true; }
	if (m_phase == 253690)
	{
		bool const bk = profile_is("mac-bk");
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const square = !bk && variant >= 73728; unsigned const index = square ? variant - 73728 : variant;
		auto const f = mac_smem_case(index, bk, square);
		unsigned const checkpoint = f.read - 1 + index / (bk ? 32768 : square ? 12288 : 24576);
		bool const absolute = u8(f.opcode) == 0xf8, destination_b = BIT(f.opcode, 8);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "MAC/MAS execute without an atomic fallback");
		if (tick == 4 && absolute) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601, 0xdead); }
		if (tick == f.read) { expect(m_cpu->state_int(tms320c54x_device::STATE_T) == (square ? f.memory : f.t), "Smem arithmetic publishes T only for square instructions in Read"); auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.address, 0xdead); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xdead); }
		capture_pipeline_at(checkpoint, {&m_pipeline_operand_reads, &m_pipeline_store_writes, &m_pipeline_extensions_reads},
			"Save MAC/MAS before/after operand capture or result publication");
		if (tick == 12)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (destination_b ? f.a : f.result) && m_cpu->state_int(tms320c54x_device::STATE_B) == (destination_b ? f.result : f.b) && m_cpu->state_int(tms320c54x_device::STATE_T) == 0xdead && m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.result_st0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1, "MAC/MAS retain memory/T, preserve the other accumulator and apply rounding/SMUL/OVM/sticky overflow");
			expect(m_pipeline_operand_reads == 1 && !m_pipeline_store_writes && m_pipeline_extensions_reads == unsigned(absolute), "MAC/MAS never repeat their sampled source or extension");
			if (bk) expect(m_cpu->state_int(tms320c54x_device::STATE_BK) == 0xdead, "MAC/MAS do not overwrite the later BK value");
			if (replay_pipeline_at(253700, checkpoint,
				"Replay MAC/MAS retained operands and result publication")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (index < (bk ? 98303 : square ? 36863 : 73727)) { start_mac_smem(variant + 1); return true; }
			if (bk) { osd_printf_info("TMS320C54x staged BK MAC/MAS: PASS cases=98304 restore=98304 operations=MAC,MACR,MAS,MASR destinations=A,B forms=direct,absolute pairs=64 flags=16 pages=2 snapshots=3 source_reads=1 data_writes=0 bk_retained=1 t_retained=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated BK MAC/MAS complete"); }
			if (!square)
			{
				osd_printf_info("TMS320C54x staged MAC/MAS: PASS cases=73728 restore=73728 operations=MAC,MACR,MAS,MASR destinations=A,B forms=indirect_RAM,absolute_RAM,mapped_AR1 pairs=64 flags=16 source_reads=1 data_writes=0 t_retained=1 silicon_phase_claim=0\n");
				start_mac_smem(73728); return true;
			}
			osd_printf_info("TMS320C54x staged SQURA/SQURS: PASS cases=36864 restore=36864 operations=add,subtract destinations=A,B forms=indirect_RAM,absolute_RAM,mapped_AR1 pairs=64 flags=16 snapshots=3 source_reads=1 data_writes=0 t_publish=Read t_republish=0 repeat_acceptance=0 irq_acceptance=0 physical_phase_claim=0\n");
			start_poly(); return true;
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 253700) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 253690; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}



bool tms320c54x_test_state::check_memory_immediate_results()
{
	if (m_phase == 254340)
	{
		auto const f = memory_immediate_case(m_pipeline_peripheral_case); unsigned const tick = ++m_idle_fixture_tick;
		expect(m_cpu->state_int(tms320c54x_device::STATE_PMST) == f.pmst, "Memory multiply preserves the configured SMUL mode");
		u64 a = f.a; if (tick > f.read && f.address == 8) a = (a & ~0xffffULL) | 0xdead; if (tick > f.read && f.address == 9) a = (a & ~0xffff0000ULL) | 0xdead0000ULL;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_A) == (tick > f.read && !f.bdst ? f.result : a) && m_cpu->state_int(tms320c54x_device::STATE_B) == (tick > f.read && f.bdst ? f.result : f.b) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == (tick > f.read ? f.result_st0 : f.st0) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1, "Memory/immediate MPY publishes signed product and sticky overflow only at Execute");
		expect(m_cpu->state_int(tms320c54x_device::STATE_T) == (tick > f.read ? 0xdead : tick == f.read || f.address == 0x0e ? f.word : 0x9abc), "MPY publishes the memory word to T during Read, independent of later T mutation");
		if (tick == 3 || (f.words == 3 && tick == 4)) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x5fe + tick, 0xf495); }
		if (tick == f.access) { expect(m_cpu->state_int(tms320c54x_device::STATE_AR2) == std::array<unsigned, 13>{0x140c, 0x140c, 0x140d, 0x140b, 0x140d, 0x140c, 0x140f, 0x1402, 0x140c, 0x140c, 0x140c, 0x140c, 0x140c}[f.form], "MPY applies its selected pointer modifier once"); m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x3333); }
		if (tick == f.read) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.address, 0xdead); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xdead); }
		if (tick == f.checkpoint && !m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save MPY across literal, address, Read and Execute ownership"); }
		if (tick == 14)
		{
			expect(m_pipeline_operand_reads == 1 && m_pipeline_extensions_reads == f.words - 1 && m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x3333 && m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == f.base + 0x601 + f.words, "MPY keeps captured address/source/literals and resumes after the complete instruction");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1]; expect(restore_snapshot(machine(), *m_check_timer, m_saved_pipeline) == STATERR_NONE, "Replay memory/immediate MPY without duplicate operand accesses"); ++m_memory_immediate_restores; m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 254350; m_check_timer->enable(true); return true; }
			unsigned const variant = m_pipeline_peripheral_case; m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			m_memory_immediate_coverage.add(memory_immediate_key(variant));
			expect(m_memory_immediate_restores == m_memory_immediate_coverage.count, "Every selected memory/immediate input completed a real restore");
			if (variant + 1 == memory_immediate_mpy_inputs) osd_printf_info("TMS320C54x memory immediate MPY: PASS cases=%u restore=%u forms=12 values=8 literals=8 flags=8 accumulators=2 pages=2 snapshots=4 checkpoint_policy=balanced source_reads=1 data_writes=0 t_publication=Read address_literal_retained=1 repeat_acceptance=0 irq_acceptance=0 silicon_phase_claim=0\n", m_memory_immediate_coverage.count, m_memory_immediate_restores);
			if (variant + 1 < memory_immediate_inputs) { start_memory_immediate_multiply(variant + 1); return true; }
			osd_printf_info("TMS320C54x memory immediate MAC: PASS cases=%u restore=%u ram_forms=8 values=8 literals=8 flags=16 selectors=4 pages=2 snapshots=4 checkpoint_policy=balanced source_reads=1 data_writes=0 t_publication=Read repeat_acceptance=0 irq_acceptance=0 silicon_phase_claim=0 coverage_cases=%u coverage_fnv64=%016llx\n", m_memory_immediate_coverage.count - memory_immediate_mpy_inputs, m_memory_immediate_restores - memory_immediate_mpy_inputs, m_memory_immediate_coverage.count, (unsigned long long)m_memory_immediate_coverage.value);
			throw emu_fatalerror(3, "TMS320C54x isolated memory immediate MPY complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254350) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254340; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_bd_long_accumulator_results()
{
	if (m_phase == 254540)
	{
		bool const mac = profile_is("mac-delay");
		bool const square = profile_is("square-delay");
		bool const memory_square = profile_is("squr-delay");
		bool const long_acc = profile_is("bd-maca-long");
		bool const acc = square || memory_square || long_acc || profile_is("maca-delay");
		bool const rounded = profile_is("mpyr-delay");
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = long_acc ? bd_long_accumulator_case(variant) : memory_square ? delay_memory_square_case(variant) : square ? delay_square_case(variant) : acc ? delay_accumulator_multiply_case(variant) : delay_multiply_case(variant, mac, rounded);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "A complete memory MPY fits its control-delay words");
		if (acc)
		{
			u64 const mask = long_acc && tick <= f.read() ? 0xffffffffffULL : 0;
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (!f.destination && tick > f.read() ? f.result : f.a ^ mask) && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.destination && tick > f.read() ? f.result : f.b ^ mask), "Delayed multiply publishes only its selected accumulator in Execute");
			if (tick == f.read() - 1) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + f.address_ar) == (!long_acc && f.form == 2 ? 0x5001 : 0x5000), "Delayed accumulator multiply owns one pointer update before poisoning");
		}
		if (((f.form && !acc) || long_acc) && tick == f.literal_clock()) { auto const disabled = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.caller + 0x601 + f.delay(), 0x5010); }
		if (tick == f.read() - 1) { if (rounded) expect(m_cpu->state_int(tms320c54x_device::STATE_AR2) == (f.form == 0 ? 0x5001 : 0x5000), "Delayed MPYR owns its postincrement before pointer poisoning"); m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + f.address_ar, 0x6000); }
		if (tick == f.read()) { expect(m_cpu->state_int(tms320c54x_device::STATE_T) == (acc ? f.memory : f.t), "Multiply owns its T publication or retained T source at Read"); auto const disabled = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.source, 0xdead); m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xdead); }
		if (long_acc && tick == f.read()) { m_cpu->set_state_int(tms320c54x_device::STATE_A,f.a); m_cpu->set_state_int(tms320c54x_device::STATE_B,f.b); }
		if (tick == f.checkpoint && !m_pipeline_replay) { m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_store_writes, m_pipeline_register_writes, m_fast_stack_reads, m_pipeline_extensions_reads}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save delayed multiply Decode, Access and retained Read operands"); }
		if (tick == 24)
		{
			u32 const continuation = f.kind == 2 ? f.caller + 0x606 : f.target + 0x702;
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(STATE_GENPC) == continuation && m_pipeline_operand_reads == 1 && m_pipeline_store_writes == 1 && m_pipeline_register_writes == (f.kind == 4 ? 2U : unsigned(f.kind == 3)) && m_fast_stack_reads == unsigned(f.kind == 5) && m_pipeline_extensions_reads == unsigned(long_acc || (f.form != 0 && !acc)) && m_cpu->state_int(tms320c54x_device::STATE_A) == (f.destination ? f.a : f.result) && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.destination ? f.result : f.b) && m_cpu->state_int(tms320c54x_device::STATE_T) == 0xdead && m_cpu->state_int(tms320c54x_device::STATE_TRN) == u16(f.result) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == ((acc ? 0xa020 : 0xa008) | (f.overflow() ? f.destination ? 0x200 : 0x400 : 0)) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == ((acc ? 0x800 | (f.flags & 8 ? 0x100 : 0) : 0x900) | (f.flags & 1 ? 0x40 : 0) | (f.flags & 2 ? 0x200 : 0)) && m_cpu->state_int(tms320c54x_device::STATE_SP) == (f.kind == 4 ? 0x2ffe : f.kind == 3 ? 0x2fff : f.kind == 5 ? 0x3001 : 0x3000), "Delayed MPY preserves independent signed arithmetic, status, frames and target page through replay");
			if (!m_pipeline_replay) { m_pipeline_replay = true; m_idle_fixture_tick = f.checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_store_writes = m_recurring_irq_snapshot[1]; m_pipeline_register_writes = m_recurring_irq_snapshot[2]; m_fast_stack_reads = m_recurring_irq_snapshot[3]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[4]; if (memory_square || long_acc) m_cpu->set_pipeline_frontend(true); return restore_pipeline_at(254550, "Replay delayed multiply without repeated source reads or callee publications"); }
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant < (long_acc ? 98303 : memory_square ? 27647 : acc ? 55295 : rounded ? 36863 : mac ? 147455 : 18431)) { start_delay_multiply(variant + 1); return true; }
			if (long_acc) { osd_printf_info("TMS320C54x BD long accumulator multiply: PASS cases=98304 restore=98304 operations=MPYA,MASA,MACA,MACAR forms=positive_offset,negative_offset,absolute address_registers=8 patterns=8 flags=16 pages=2 snapshots=4 extension_reads=1 source_reads=1 accumulators=Execute t_publish=Read target_observes_result=1 reset_before_restore=1 physical_phase_claim=0\n"); throw emu_fatalerror(3,"TMS320C54x isolated BD long accumulator multiply complete"); }
			if (memory_square) { osd_printf_info("TMS320C54x delayed memory SQUR: PASS cases=27648 restore=27648 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD destinations=2 forms=DP,indirect,postincrement patterns=8 frct_ovm_smul_sxm=16 pages=2 snapshots=3 source_reads=1 t_publication=Read newer_t_preserved=1 smul_product_clamp=0 target_observes_result=1 reset_before_restore=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delayed memory SQUR complete"); }
			if (square) { osd_printf_info("TMS320C54x delayed square accumulation: PASS cases=55296 restore=55296 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD operations=SQURA,SQURS destinations=2 forms=DP,indirect,postincrement patterns=8 frct_ovm_smul_sxm=16 pages=2 snapshots=3 source_reads=1 t_publication=Read smul_product_clamp=0 target_observes_result=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delayed square accumulation complete"); }
			if (rounded) { osd_printf_info("TMS320C54x delayed MPYR: PASS cases=36864 restore=36864 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD forms=postincrement,absolute_RAM,absolute_AR7,offset destinations=2 operand_pairs=16 frct_ovm_smul=8 pages=2 snapshots=3 source_reads=1 rounding=biased_high_word smul_product_clamp=0 t_retained=1 callee_observes_result=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delayed MPYR complete"); }
			if (acc) { osd_printf_info("TMS320C54x delayed accumulator multiply: PASS cases=55296 restore=55296 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD operations=MPYA,MASA,MACA,MACAR forms=DP,indirect,postincrement patterns=8 frct_ovm_smul_sxm=16 pages=2 snapshots=3 source_reads=1 t_publication=Read target_observes_result=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delayed accumulator multiply complete"); }
			if (mac) { osd_printf_info("TMS320C54x staged delay MAC/MAS: PASS cases=147456 restore=147456 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD operations=MAC,MACR,MAS,MASR forms=short,absolute_RAM,absolute_AR5,offset destinations=2 operand_pairs=16 frct_ovm_smul=8 pages=2 snapshots=3 source_reads=1 t_retained=1 callee_observes_result=1 physical_phase_claim=0\n"); throw emu_fatalerror(3, "TMS320C54x isolated delay MAC/MAS complete"); }
			osd_printf_info("TMS320C54x staged delay MPY: PASS cases=18432 restore=18432 controls=BD,BCD_taken,BCD_untaken,CALLD,FCALLD,RETD forms=short,absolute_RAM,absolute_AR,offset destinations=2 operand_pairs=16 frct_ovm=4 pages=2 snapshots=3 source_reads=1 callee_observes_result=1 silicon_phase_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x isolated delay MPY complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254550) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254540; m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}





bool tms320c54x_test_state::check_delay_compare_results()
{
	if (m_phase == 254980)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick; auto const f = delay_compare_case(variant);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Complete short CMPM BITF is admitted in a BD BCD delay span");
		if (tick == 5 + f.prefix && f.interrupt) m_cpu->set_input_line(f.interrupt == 2 ? INPUT_LINE_NMI : 0, ASSERT_LINE);
		if (tick == f.literal_clock) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x603 + f.prefix, 0xdead); m_cpu->space(AS_PROGRAM).write_word(f.base + 0x601 + f.prefix, 0xbeef); }
		if (tick == f.read - 1 && f.form == 2) { expect(m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x1003, "Delay comparison updates its pointer once in Access"); m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x2200); }
		if (tick == f.read) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(0x1002, 0xdead); }
		capture_pipeline_at(f.checkpoint, {&m_pipeline_operand_reads, &m_pipeline_extensions_reads, &m_vector_acks, &m_pipeline_store_writes},
			"Save delayed comparison with settled IRQ and retained operand ownership");
		if (tick == 64)
		{
			// A pending IRQ may wake the prefetched IDLE and return after it.
			u64 const pc = m_cpu->state_int(STATE_GENPC);
			bool const on_path = pc >= f.base + (f.taken ? 0x700 : 0x604 + f.prefix) && pc < f.base + (f.taken ? 0x740 : 0x640);
			bool const state_ok = on_path && m_cpu->state_int(tms320c54x_device::STATE_B) == (f.taken ? 0x11 : 0x22) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == (0xae20 | (f.final_tc ? 0x1000 : 0)) && m_cpu->state_int(tms320c54x_device::STATE_ST1) == (0x100 | (f.form == 1 ? 0x4000 : 0)) && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1000 && m_cpu->state_int(tms320c54x_device::STATE_A) == 0x123456789aULL && m_cpu->state_int(tms320c54x_device::STATE_T) == 0x5aa5 && m_cpu->state_int(tms320c54x_device::STATE_AR2) == (f.form == 2 ? 0x2200 : 0x1002);
			if (!state_ok) osd_printf_info("delay_compare_state: case=%u replay=%u pc=%llx idle=%llu a=%llx b=%llx st0=%llx sp=%llx ar2=%llx t=%llx taken=%u tc=%u reads=%u literals=%u acks=%u\n", variant, unsigned(m_pipeline_replay), (unsigned long long)m_cpu->state_int(STATE_GENPC), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_IDLE), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_ST0), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_SP), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_AR2), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_T), unsigned(f.taken), unsigned(f.final_tc), m_pipeline_operand_reads, m_pipeline_extensions_reads, m_vector_acks);
			expect(state_ok, "Delay comparison publishes TC without changing the retained branch decision or duplicating pointer updates");
			expect(m_pipeline_operand_reads == (f.prefix ? 2U : 1U) && m_pipeline_extensions_reads == 2 + unsigned(bool(f.prefix)) && !m_pipeline_store_writes && m_vector_acks == unsigned(bool(f.interrupt)), "Delay comparison retains literal and source across one deferred IRQ NMI entry");
			if (replay_pipeline_at(254990, f.checkpoint,
				"Restore delay comparison and deferred interrupt without repeated transactions")) return true;
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_input_line(0, CLEAR_LINE); m_cpu->set_input_line(INPUT_LINE_NMI, CLEAR_LINE); m_cpu->set_pipeline_frontend(false);
			if (variant < 27647) { start_delay_compare(variant + 1); return true; }
			osd_printf_info("TMS320C54x delayed CMPM BITF: PASS cases=27648 restore=27648 controls=BD,BCD_TC,BCD_NTC operations=2 forms=DP,SP,postincrement value_pairs=16 initial_tc=2 prefix=none,BITF sources=none,IRQ,NMI pages=2 snapshots=4 branch_decision=retained literal_reads=once operand_writes=0 interrupt_after_publication=1 physical_phase_claim=0\n"); throw emu_fatalerror(0, "TMS320C54x isolated delayed comparison complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 254990) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 254980; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}


bool tms320c54x_test_state::check_mapped_alu_results()
{
	if (m_phase == 62156)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const arithmetic = profile_is("unsigned-map");
		bool const shift16 = profile_is("shift16-map");
		bool const logic = profile_is("logic-map");
		unsigned const extent = shift16 || logic ? 36864 : 110592; auto const f = logic ? mapped_logic_case(variant) : shift16 ? mapped_shift16_case(variant) : addc_case(variant % extent, arithmetic ? 1 + variant / extent : 0);
		unsigned const checkpoint = f.read_clock - 1 + f.snapshot;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "Owned unsigned or logical source remains admitted");
		if (tick == f.read_clock)
		{
			expect(m_pipeline_operand_reads == 1 && m_cpu->state_int(tms320c54x_device::STATE_AR3) == f.next_ar3, "Unsigned arithmetic resolves and updates its address once before operand mutation");
			set_repeat_count_source(f.address, 0xa55a);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, f.after_a); m_cpu->set_state_int(tms320c54x_device::STATE_B, f.after_b); m_cpu->set_state_int(tms320c54x_device::STATE_ST0, f.execute_st0);
			for (unsigned i = 0; i < 8; ++i) m_cpu->set_state_int(tms320c54x_device::STATE_AR0 + i, 0x3330 + i);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save unsigned arithmetic before Read, after capture or after Execute");
		}
		if (tick == 12)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_pipeline_operand_reads == 1U && !m_pipeline_store_writes && m_pipeline_extensions_reads == unsigned(f.extension), "Unsigned arithmetic owns one source and optional extension but no memory write");
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == f.final_a && m_cpu->state_int(tms320c54x_device::STATE_B) == f.final_b && m_cpu->state_int(tms320c54x_device::STATE_ST0) == f.final_st0 && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1, "Unsigned arithmetic uses retained Smem and Execute-live accumulator with sticky overflow and OVM");
			for (unsigned i = 0; i < 8; ++i) expect(m_cpu->state_int(tms320c54x_device::STATE_AR0 + i) == (0x3330 + i), "Unsigned arithmetic never reapplies an auxiliary update after Read or restore");
			expect(m_cpu->state_int(tms320c54x_device::STATE_T) == (f.address == 14 ? 0xa55a : f.t) && m_cpu->state_int(tms320c54x_device::STATE_TRN) == (f.address == 15 ? 0xa55a : f.trn) && m_cpu->state_int(tms320c54x_device::STATE_BK) == f.bk && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x3000, "Unsigned arithmetic preserves unselected registers and does not rewrite a mutated mapped operand");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1]; if (shift16 || logic) m_cpu->set_pipeline_frontend(true); return restore_pipeline_at(62157, "Restore unsigned arithmetic without duplicated reads or address updates");
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant + 1 < extent * (logic ? 3U : shift16 ? 4U : arithmetic ? 2U : 1U)) { start_addc(variant + 1); return true; }
			if (logic)
			{
				osd_printf_info("TMS320C54x mapped memory logic: PASS cases=110592 restore=110592 operations=AND,OR,XOR mapped_sources=16 modes=DP,absolute,indirect accumulators=2 flags=8 patterns=4 late_accumulator=2 pages=2 snapshots=3 zero_extend=1 source_sample=Read accumulator_sample=Execute flags_preserved=1 writes=0 reset_before_restore=1 physical_phase_claim=0 producer_control_composition=unvalidated\n"); throw emu_fatalerror(3, "TMS320C54x isolated mapped memory logic complete");
			}
			if (shift16)
			{
				osd_printf_info("TMS320C54x mapped shift16 arithmetic: PASS cases=147456 restore=147456 forms=8 mapped_sources=16 modes=DP,absolute,indirect flags=8 patterns=4 late_source=2 pages=2 snapshots=3 source_sample=Read accumulator_sample=Execute partial_carry=1 sticky_overflow=1 ovm=1 writes=0 reset_before_restore=1 physical_phase_claim=0 producer_control_composition=unvalidated\n"); throw emu_fatalerror(3, "TMS320C54x isolated mapped shift16 arithmetic complete");
			}
			if (arithmetic)
			{
				osd_printf_info("TMS320C54x unsigned mapped arithmetic: PASS cases=221184 restore=221184 operations=ADDS,SUBS numeric_cases=147456 mapped_cases=73728 ram_forms=6 mapped_sources=16 accumulators=2 carry_inputs=ignored sxm_ovm=4 words=8 accumulator_patterns=8 late_accumulator=2 pages=2 snapshots=3 unsigned_smem=1 physical_phase_claim=0 control_composition=unvalidated\n"); throw emu_fatalerror(3, "TMS320C54x isolated unsigned mapped arithmetic complete");
			}
			osd_printf_info("TMS320C54x ADDC: PASS cases=110592 restore=110592 numeric_cases=73728 mapped_cases=36864 ram_forms=6 mapped_sources=16 accumulators=2 carry_states=2 sxm_ovm=4 words=8 accumulator_patterns=8 late_accumulator_carry=2 pages=2 snapshots=3 unsigned_smem=1 physical_phase_claim=0 control_composition=unvalidated\n"); throw emu_fatalerror(3, "TMS320C54x isolated ADDC complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62157) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62156; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}

bool tms320c54x_test_state::check_sqdst_results()
{
	if (m_phase == 62142)
	{
		unsigned const variant = m_pipeline_peripheral_case, tick = ++m_idle_fixture_tick;
		bool const lms = !strncmp(machine().options().bios(), "lms", 3);
		auto const c = sqdst_case(variant, lms); auto const &f = c.memory;
		unsigned const checkpoint = 3 + f.snapshot;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL), "SQDST admits its RAM operands");
		u64 const mask = tick <= 5 ? 0xffffffffffULL : 0;
		expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (tick >= 6 ? c.result_a : c.a ^ mask) && m_cpu->state_int(tms320c54x_device::STATE_B) == (tick >= 6 ? c.result_b : c.b ^ mask) && m_cpu->state_int(tms320c54x_device::STATE_ST0) == (tick >= 6 ? c.result_st0 : f.st0), "SQDST computes both results from Execute-owned old A/B and retains independent overflow flags");
		if (tick == 4)
		{
			for (unsigned i = 0; i < 4; ++i) { expect(m_cpu->state_int(tms320c54x_device::STATE_AR2 + i) == f.final[i], "SQDST updates aliased pointers with X priority"); m_cpu->set_state_int(tms320c54x_device::STATE_AR2 + i, 0x2200 + i); }
			m_cpu->set_state_int(tms320c54x_device::STATE_BK, 32);
		}
		if (tick == 5)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_T) == 0xbeef, "SQDST does not publish T");
			m_cpu->set_state_int(tms320c54x_device::STATE_T, 0xdead); m_cpu->set_state_int(tms320c54x_device::STATE_A, c.a); m_cpu->set_state_int(tms320c54x_device::STATE_B, c.b);
			auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(f.xaddress, 0xdead); m_cpu->space(AS_DATA).write_word(f.yaddress, 0xbeef);
		}
		if (tick == checkpoint && !m_pipeline_replay)
		{
			m_recurring_irq_snapshot = {m_pipeline_operand_reads, m_pipeline_extensions_reads}; expect(capture_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Save SQDST around address capture and paired Read");
		}
		if (tick == 12)
		{
			expect(m_pipeline_operand_reads == 2 && m_pipeline_extensions_reads == 1 && m_cpu->state_int(tms320c54x_device::STATE_IDLE) && m_cpu->state_int(tms320c54x_device::STATE_T) == 0xdead && m_cpu->state_int(tms320c54x_device::STATE_ST1) == f.st1 && m_cpu->state_int(tms320c54x_device::STATE_PMST) == f.pmst, "SQDST retains memory samples without changing T or mode bits");
			for (unsigned i = 0; i < 4; ++i) expect(m_cpu->state_int(tms320c54x_device::STATE_AR2 + i) == 0x2200 + i, "SQDST does not repeat pointer updates");
			if (!m_pipeline_replay)
			{
				m_pipeline_replay = true; m_idle_fixture_tick = checkpoint; m_pipeline_operand_reads = m_recurring_irq_snapshot[0]; m_pipeline_extensions_reads = m_recurring_irq_snapshot[1];
				if (lms) m_cpu->set_pipeline_frontend(true);
				m_check_timer->adjust(attotime::never); expect(restore_snapshot(machine(), m_saved_pipeline) == STATERR_NONE, "Restore SQDST without duplicate reads or publications"); m_cpu->suspend(SUSPEND_REASON_DISABLE, true); m_phase = 62143; m_check_timer->enable(true); return true;
			}
			m_pipeline_operand_tap.remove(); m_pipeline_write_tap.remove(); m_pipeline_fetch_tap.remove(); m_cpu->set_pipeline_frontend(false);
			if (variant + 1 < 402432U) { start_sqdst(variant + 1); return true; }
			if (lms)
			{
				osd_printf_info("TMS320C54x staged LMS: PASS address_cases=9216 arithmetic_cases=393216 restore=402432 encodings=256 layouts=3 bk_modes=2 value_pairs=64 old_a_patterns=8 old_b_patterns=8 arithmetic_flags=32 snapshots=3 reads=2 writes=0 t_unchanged=1 old_accumulators=Execute physical_banks=unvalidated interrupt_repeat_composition=unvalidated\n"); throw emu_fatalerror(3,"TMS320C54x isolated LMS complete");
			}
			osd_printf_info("TMS320C54x staged SQDST: PASS address_cases=9216 arithmetic_cases=393216 restore=402432 encodings=256 layouts=3 bk_modes=2 value_pairs=64 old_a_patterns=8 old_b_patterns=8 arithmetic_flags=32 snapshots=3 reads=2 writes=0 t_unchanged=1 old_accumulators=Execute physical_banks=unvalidated interrupt_repeat_composition=unvalidated\n"); throw emu_fatalerror(3, "TMS320C54x isolated SQDST complete");
		}
		m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
	}
	if (m_phase == 62143) { m_cpu->resume(SUSPEND_REASON_DISABLE); m_phase = 62142; m_check_timer->enable(true); m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true; }
	return false;
}




} // namespace c54x_test
