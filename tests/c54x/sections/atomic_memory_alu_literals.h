// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	struct row { u16 opcode,address,operand,st1; u64 a,b,out_a,out_b; u16 flags; bool increment; };
	constexpr row rows[]{
		{0x0882,0xf22,0x8001,0x100,0x10000,0x12345678,0x17fff,0x12345678,0,false},
		{0x0881,0xf21,0x8001,0x100,0x10000,0x12345678,0x17fff,0x12345678,0,false},
		{0x0881,0xf21,0x8001,0,0x10000,0x12345678,0x7fff,0x12345678,0x800,false},
		{0x0092,0xf22,0xffff,0x100,1,0x12345678,0,0x12345678,0x800,true},
		{0x0092,0xf22,0xffff,0,1,0x12345678,0x10000,0x12345678,0,true},
		{0x0192,0xf22,0xffff,0x100,0x12345678,1,0x12345678,0,0x800,true},
		{0x0192,0xf22,0xffff,0,0x12345678,1,0x12345678,0x10000,0,true},
		{0x1b84,0xf24,0xf0,0x100,0x7fff,0xff0000000fULL,0x7fff,0xff000000ffULL,0x800,false},
		{0x1bf8,0xf24,0xf0,0x100,0x7fff,0xff0000000fULL,0x7fff,0xff000000ffULL,0x800,false}
	};
	std::vector<clocked_register_case> cases;
	for (auto const &v : rows) {
		bool const extended = v.opcode == 0x1bf8;
		auto initial = initial_case("memory-SUB-ADD-OR-SXM-and-pointer-literals", {0xf495,v.opcode});
		if (extended) initial.program[2].value = v.address;
		initial.program[2 + extended].value = 0xf4e1;
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,v.a},{r::STATE_B,v.b},{r::STATE_AR1,0xf21},
			{r::STATE_AR2,0xf22},{r::STATE_AR4,0xf24},{r::STATE_ST0,v.opcode == 0x1b84 || extended ? 0x800U : 0U},{r::STATE_ST1,v.st1}});
		initial.data = {{0xf21,0x8001},{0xf22,0x1234},{0xf24,0xf0}};
		for (auto &word : initial.data) if (word.address == v.address) word.value = v.operand;
		auto final = observation("signedness-carry-pointer-and-unrelated-source", {{STATE_GENPC,0x603 + unsigned(extended)},
			{r::STATE_A,v.out_a},{r::STATE_B,v.out_b},{r::STATE_AR1,0xf21},{r::STATE_AR2,v.increment ? 0xf23U : 0xf22U},
			{r::STATE_AR4,0xf24},{r::STATE_ST0,v.flags},{r::STATE_ST1,v.st1}});
		final.expected_data = initial.data;
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-ALU-snapshot", {{STATE_GENPC,0x601}})},
			{2,observation("ALU-running", {})},{32,std::move(final)}},1,32};
		c.pipeline_frontend = false; c.data_events = std::vector<clocked_data_event>{{false,v.address,v.operand,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
