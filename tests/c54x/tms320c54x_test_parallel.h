// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_PARALLEL_H
#define MAME_NOKIA_TMS320C54X_TEST_PARALLEL_H

#include "tms320c54x_test_case.h"
#include "cpu/tms320c54x/tms320c54x.h"
#include <array>

namespace c54x_test {

// SPRU172C parallel ST||MPY/ADD/SUB/LD examples and interaction controls.
// Literal expectations distinguish old-source stores, same-cell read-before-
// write, independent address updates and saturation. No CPU helper is reused.
struct parallel_vector
{
	const char *id;
	u16 opcode;
	std::vector<memory_word> data;
	std::vector<register_setting> initial;
	std::vector<register_check> expected;
	std::vector<memory_word> expected_data;
};

inline auto const &parallel_vectors()
{
	using r = tms320c54x_device;
	static const std::array<parallel_vector, 10> cases{{
		{"st-mpy-fract", 0xcdb9, {{0x0200, 0x1111}, {0x0300, 0x4000}},
		 {{r::STATE_A, 0xff84211234ULL}, {r::STATE_B, 0}, {r::STATE_T, 0x4000},
		  {r::STATE_AR3, 0x0200}, {r::STATE_AR5, 0x0300}, {r::STATE_ST1, 0x0140}},
		 {{r::STATE_A, 0xff84211234ULL}, {r::STATE_B, 0x20000000}, {r::STATE_T, 0x4000},
		  {r::STATE_AR3, 0x0201}, {r::STATE_AR5, 0x0301}}, {{0x0200, 0x8421}, {0x0300, 0x4000}}},
		{"st-mpy-old-destination", 0xcfb9, {{0x0200, 0}, {0x0300, 3}},
		 {{r::STATE_A, 0xabcdef}, {r::STATE_B, 0x12345678}, {r::STATE_T, 2},
		  {r::STATE_AR3, 0x0200}, {r::STATE_AR5, 0x0300}, {r::STATE_ST1, 0x0100}},
		 {{r::STATE_A, 0xabcdef}, {r::STATE_B, 6}, {r::STATE_T, 2},
		  {r::STATE_AR3, 0x0201}, {r::STATE_AR5, 0x0301}}, {{0x0200, 0x1234}}},
		{"st-add-opposite", 0xc131, {{0x0200, 0}, {0x0300, 2}},
		 {{r::STATE_A, 0x12345678}, {r::STATE_B, 0xdead}, {r::STATE_AR3, 0x0200},
		  {r::STATE_AR5, 0x0300}, {r::STATE_ST1, 0x0100}},
		 {{r::STATE_A, 0x12345678}, {r::STATE_B, 0x12365678},
		  {r::STATE_AR3, 0x0200}, {r::STATE_AR5, 0x0300}}, {{0x0200, 0x1234}}},
		{"st-sub-circular-asm", 0xc5f5, {{0x01ff, 0x1111}, {0x0300, 0x8001}},
		 {{r::STATE_A, 0xff84210000ULL}, {r::STATE_B, 0x0010000001ULL}, {r::STATE_AR0, 2},
		  {r::STATE_AR3, 0x01ff}, {r::STATE_AR5, 0x0300}, {r::STATE_BK, 4},
		  {r::STATE_PMST, 0}, {r::STATE_ST1, 0x0101}},
		 {{r::STATE_A, 0xff84210000ULL}, {r::STATE_B, 0xfffbe00000ULL},
		  {r::STATE_AR3, 0x01fe}, {r::STATE_AR5, 0x0302}}, {{0x01ff, 0x0842}}},
		{"st-ld-signed", 0xca31, {{0x0200, 0}, {0x0300, 0x8001}},
		 {{r::STATE_A, 0}, {r::STATE_B, 0x12345678}, {r::STATE_AR3, 0x0200},
		  {r::STATE_AR5, 0x0300}, {r::STATE_PMST, 0}, {r::STATE_ST1, 0x0100}},
		 {{r::STATE_A, 0xff80010000ULL}, {r::STATE_B, 0x12345678},
		  {r::STATE_AR3, 0x0200}, {r::STATE_AR5, 0x0300}}, {{0x0200, 0x1234}}},
		{"st-ld-t-same-cell", 0xe411, {{0x0200, 0x80ff}},
		 {{r::STATE_A, 0x12345678}, {r::STATE_T, 0}, {r::STATE_AR3, 0x0200}, {r::STATE_ST1, 0x0100}},
		 {{r::STATE_A, 0x12345678}, {r::STATE_T, 0x80ff}, {r::STATE_AR3, 0x0200}}, {{0x0200, 0x1234}}},
		{"st-a-ld-t-circular", 0xe4e9, {{0x0203, 0x80ff}, {0x0300, 0xbeef}},
		 {{r::STATE_A, 0x12345678}, {r::STATE_B, 0x87654321}, {r::STATE_T, 0}, {r::STATE_AR0, 1},
		  {r::STATE_AR3, 0x0300}, {r::STATE_AR4, 0x0203}, {r::STATE_BK, 4},
		  {r::STATE_ST0, 0x0aa5}, {r::STATE_ST1, 0x0100}},
		 {{r::STATE_A, 0x12345678}, {r::STATE_B, 0x87654321}, {r::STATE_T, 0x80ff},
		  {r::STATE_AR3, 0x0301}, {r::STATE_AR4, 0x0200}, {r::STATE_ST0, 0x0aa5}},
		 {{0x0300, 0x1234}, {0x0203, 0x80ff}}},
		{"st-b-ld-t-circular", 0xe6e9, {{0x0203, 0x80ff}, {0x0300, 0xbeef}},
		 {{r::STATE_A, 0x12345678}, {r::STATE_B, 0x87654321}, {r::STATE_T, 0}, {r::STATE_AR0, 1},
		  {r::STATE_AR3, 0x0300}, {r::STATE_AR4, 0x0203}, {r::STATE_BK, 4},
		  {r::STATE_ST0, 0x0aa5}, {r::STATE_ST1, 0x0100}},
		 {{r::STATE_A, 0x12345678}, {r::STATE_B, 0x87654321}, {r::STATE_T, 0x80ff},
		  {r::STATE_AR3, 0x0301}, {r::STATE_AR4, 0x0200}, {r::STATE_ST0, 0x0aa5}},
		 {{0x0300, 0x8765}, {0x0203, 0x80ff}}},
		{"st-add-saturate", 0xc131, {{0x0200, 0}, {0x0300, 1}},
		 {{r::STATE_A, 0x7fffffff}, {r::STATE_B, 0}, {r::STATE_ST0, 0x0800},
		  {r::STATE_ST1, 0x0300}, {r::STATE_AR3, 0x0200}, {r::STATE_AR5, 0x0300}},
		 {{r::STATE_B, 0x7fffffff}, {r::STATE_ST0, 0x0200, 0x0e00}}, {{0x0200, 0x7fff}}},
		{"st-sub-saturate", 0xc531, {{0x0200, 0}, {0x0300, 0x8000}},
		 {{r::STATE_A, 0x00010000}, {r::STATE_B, 0}, {r::STATE_ST0, 0},
		  {r::STATE_ST1, 0x0300}, {r::STATE_AR3, 0x0200}, {r::STATE_AR5, 0x0300}},
		 {{r::STATE_B, 0xff80000000ULL}, {r::STATE_ST0, 0x0a00, 0x0e00}}, {{0x0200, 1}}},
	}};
	return cases;
}

inline instruction_case parallel_case(unsigned index)
{
	using r = tms320c54x_device;
	auto const &v = parallel_vectors().at(index);
	instruction_case test{v.id, v.opcode,
		{{0x05e0, 0x75d6}, {0x05e1, 0x0124}, {0x05e2, v.opcode},
		 {0x05e3, 0x75d6}, {0x05e4, 0x0124}, {0x05e5, 0xf5e1}},
		v.data, v.initial, v.expected, 2, 3, v.expected_data};
	test.initial.insert(test.initial.end(), {{r::STATE_AR6, 0x0a03},
		{r::STATE_PC, 0x05e0}, {r::STATE_IDLE, 0}});
	test.expected.insert(test.expected.end(), {{r::STATE_IDLE, 1}, {r::STATE_ILLEGAL, 0}});
	return test;
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_PARALLEL_H
