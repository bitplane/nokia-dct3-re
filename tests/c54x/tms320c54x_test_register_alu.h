// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_REGISTER_ALU_H
#define MAME_NOKIA_TMS320C54X_TEST_REGISTER_ALU_H

#include "tms320c54x_test_case.h"
#include <array>
#include <string_view>

namespace c54x_test {

enum class register_alu_kind { none, minmax, square };
inline register_alu_kind select_register_alu(std::string_view name)
{
	return name == "minmax" ? register_alu_kind::minmax : name == "squr" ? register_alu_kind::square : register_alu_kind::none;
}
constexpr unsigned register_alu_count(register_alu_kind kind)
{
	return kind == register_alu_kind::square ? 4096 : 19456;
}
constexpr unsigned minmax_checkpoint(unsigned input)
{
	return (input % 16 / 2 + (input / 16) % 16 + (input / 256) % 4 + (input / 1024) % 8 + input / 8192) % 4;
}
constexpr unsigned register_alu_key(unsigned ordinal, register_alu_kind kind)
{
	if (kind == register_alu_kind::square)
		// B is not an operand: two opposite-sign sentinels, all four checkpoints.
		return ordinal % 16 + 16 * (11 + (ordinal / 16) % 2) + 256 * ((ordinal / 32) % 2) +
			512 * ((ordinal / 64) % 8) + 4096 * ((ordinal / 512) % 2) + 8192 * (ordinal / 1024);
	if (ordinal < 16384) return ordinal + 16384 * minmax_checkpoint(ordinal);
	// Preserve every tied input at all four checkpoints, not just the balanced
	// one in the numeric bank. Equality's select-B/carry rule stays explicit.
	unsigned extra = ordinal - 16384, copy = extra % 3; extra /= 3;
	unsigned const a = extra % 16; extra /= 16;
	unsigned const input = a + 16 * a + 256 * (extra % 4) + 1024 * ((extra / 4) % 8) + 8192 * (extra / 32);
	unsigned const selected = minmax_checkpoint(input);
	return input + 16384 * (copy + unsigned(copy >= selected));
}

struct minmax_fixture
{
	unsigned selector, snapshot;
	u32 base;
	u64 a, b, result;
	u16 st0, st1, result_st0;
};

inline minmax_fixture minmax_case(unsigned v, bool square)
{
	static constexpr std::array<u64, 16> values{0, 1, 0xffffffffffULL, 0x7fffffff, 0x80000000, 0xff80000000ULL, 0xff7fffffffULL, 0x7fffffffffULL, 0x8000000000ULL, 0x0180000000ULL, 0xfe7fffffffULL, 0x123456789aULL, 0xedcba98765ULL, 0x7fffff, 0xffffff, 0xffff000000ULL};
	minmax_fixture f{};
	f.a = values[v % 16]; f.b = values[(v / 16) % 16]; f.selector = (v / 256) % 4;
	unsigned const flags = (v / 1024) % 8;
	f.st0 = 0x0002 | (flags & 1 ? 0x800 : 0) | (v & 1 ? 0x600 : 0);
	f.st1 = 0x800 | (flags & 2 ? 0x200 : 0) | (flags & 4 ? 0x100 : 0);
	s64 const a = BIT(f.a, 39) ? s64(f.a) - (s64(1) << 40) : s64(f.a);
	s64 const b = BIT(f.b, 39) ? s64(f.b) - (s64(1) << 40) : s64(f.b);
	bool const choose_a = f.selector & 2 ? a < b : a > b;
	f.result = choose_a ? f.a : f.b; f.result_st0 = (f.st0 & ~0x800) | (choose_a ? 0 : 0x800);
	f.base = (v / 8192) % 2 ? 0x20000 : 0; f.snapshot = v / 16384;
	if (square)
	{
		static constexpr std::array<u32, 16> high_words{0, 1, 0xffff, 0x10000, 0x1ffff, 0x7fff, 0x8000, 0x17fff, 0x18000, 0xfffe, 0x10001, 0x8001, 0x17ffe, 0x5555, 0x15555, 0xaaaa};
		f.a = (u64(high_words[v % 16]) << 16) | 0x5aa5;
		f.st1 |= flags & 1 ? 0x40 : 0;
		u32 const bits = (f.a >> 16) & 0x1ffff;
		s64 const operand = bits >= 0x10000 ? s64(bits) - 0x20000 : bits;
		u64 const product = u64(operand * operand) * (flags & 1 ? 2 : 1);
		bool const overflow = product > 0x7fffffff;
		f.result = overflow && (flags & 2) ? 0x7fffffff : product;
		f.result_st0 = f.st0 | (overflow ? (f.selector & 1 ? 0x200 : 0x400) : 0);
	}
	return f;
}

template<typename R>
clocked_register_case register_alu_case(unsigned ordinal, register_alu_kind kind)
{
	bool const square = kind == register_alu_kind::square;
	unsigned const key = register_alu_key(ordinal, kind);
	auto const f = minmax_case(square ? key % 512 + (key / 512) * 1024 : key, square);
	u16 const opcode = square ? 0xf48d | ((f.selector & 1) << 8) : 0xf486 | ((f.selector & 1) << 8) | (f.selector >> 1);
	clocked_register_case result{{}, std::array<unsigned, 4>{3, 5, 6, 8}[f.snapshot], 12};
	for (unsigned retired = 0; retired < 2; ++retired)
	{
		u64 const a = !retired ? f.a ^ 0xffffffffffULL : !(f.selector & 1) ? f.result : f.a;
		u64 const b = !retired ? f.b ^ 0xffffffffffULL : f.selector & 1 ? f.result : f.b;
		instruction_case state{square ? "squr" : "minmax", opcode, {}, {}, {},
			{{R::STATE_A, a}, {R::STATE_B, b}, {R::STATE_ST0, retired ? f.result_st0 : f.st0},
			 {R::STATE_ST1, f.st1}, {R::STATE_T, 0x5aa5}, {R::STATE_ILLEGAL, 0}}, 0, 0};
		state.reset_before = false;
		result.states.push_back({retired ? 6U : 0U, std::move(state)});
	}
	auto &initial = result.states.front().observation;
	for (unsigned i = 0; i < 16; ++i) initial.program.push_back({f.base + 0x600 + i, 0xf495});
	initial.program[0].value = opcode; initial.program[1].value = 0xf4e1;
	for (auto const &value : initial.expected) initial.initial.push_back({value.index, value.value});
	initial.initial.insert(initial.initial.end(), {{R::STATE_IDLE, 0}, {R::STATE_PMST, 0}, {R::STATE_TCR, 0x10},
		{R::STATE_IFR, 0}, {R::STATE_IMR, 0}, {STATE_GENPC, f.base + 0x600}});
	result.actions = {{5, {{R::STATE_A, f.a}, {R::STATE_B, f.b}}}};
	result.identity = key; result.single_fetch = f.base + 0x600; result.forbid_data_access = true;
	return result;
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_REGISTER_ALU_H
