// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_SQUARE_H
#define MAME_NOKIA_TMS320C54X_TEST_SQUARE_H

#include "tms320c54x_test_case.h"
#include "cpu/tms320c54x/tms320c54x.h"
#include <array>

namespace c54x_test {

// SPRU172C SQUR/SQURA/SQURS instruction examples and signed fractional
// saturation. Expected results are literal, not computed by CPU helpers.
struct square_vector
{
	const char *id;
	u16 opcode, memory, st0, st1, t;
	u64 a, b, result_a, result_b;
	u16 result_t, flag_mask, flags;
	bool check_a, check_b, check_ar;
};

inline constexpr std::array<square_vector, 6> square_vectors{{
	{"squr-fract-saturate", 0x2682, 0x8000, 0, 0x0240, 0, 0, 0, 0x7fffffff, 0, 0x8000, 0x0400, 0x0400, true, false, true},
	{"squr-absolute-b", 0x27f8, 0xfffe, 0, 0x0100, 0, 0x1234, 0, 0x1234, 4, 0xfffe, 0, 0, true, true, false},
	{"squr-register-a", 0xf48d, 0, 0, 0x0100, 0xbeef, 0xfffffe0000ULL, 0x1234, 4, 0x1234, 0xbeef, 0, 0, true, true, false},
	{"squr-register-b-saturate", 0xf58d, 0, 0, 0x0240, 0xbeef, 0xff80000000ULL, 0, 0xff80000000ULL, 0x7fffffff, 0xbeef, 0x0200, 0x0200, true, true, false},
	{"squra-preserve-carry", 0x3882, 0xfffe, 0x0800, 0x0100, 0, 10, 0x1234, 14, 0x1234, 0xfffe, 0x0800, 0x0800, true, true, true},
	{"squrs-absolute-b", 0x3bf8, 3, 0, 0x0100, 0, 0x1234, 5, 0x1234, 0xfffffffffcULL, 3, 0, 0, true, true, false},
}};

inline instruction_case square_case(unsigned index)
{
	using cpu = tms320c54x_device;
	auto const &v = square_vectors.at(index);
	bool const absolute = (v.opcode & 0xff) == 0xf8;
	u32 const after = absolute ? 0x05e4 : 0x05e3;
	instruction_case test{v.id, v.opcode,
		{{0x05e0, 0x75d6}, {0x05e1, 0x0124}, {0x05e2, v.opcode},
		 {after, 0x75d6}, {after + 1, 0x0124}, {after + 2, 0xf5e1}},
		{{0x0f90, v.memory}},
		{{cpu::STATE_A, v.a}, {cpu::STATE_B, v.b}, {cpu::STATE_T, v.t},
		 {cpu::STATE_ST0, v.st0}, {cpu::STATE_ST1, v.st1}, {cpu::STATE_AR2, 0x0f90},
		 {cpu::STATE_AR6, 0x0a03}, {cpu::STATE_PC, 0x05e0}, {cpu::STATE_IDLE, 0}},
		{{cpu::STATE_T, v.result_t}, {cpu::STATE_IDLE, 1}, {cpu::STATE_ILLEGAL, 0}},
		2, absolute ? 4U : 3U};
	if (absolute) test.program.push_back({0x05e3, 0x0f90});
	if (v.check_a) test.expected.push_back({cpu::STATE_A, v.result_a});
	if (v.check_b) test.expected.push_back({cpu::STATE_B, v.result_b});
	if (v.check_ar) test.expected.push_back({cpu::STATE_AR2, 0x0f90});
	if (v.flag_mask) test.expected.push_back({cpu::STATE_ST0, v.flags, v.flag_mask});
	return test;
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_SQUARE_H
