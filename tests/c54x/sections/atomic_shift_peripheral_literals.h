// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	struct row { u16 opcode,extension,st0,st1,operand; u64 a,b,out_a,out_b; u16 flags,mask; unsigned interval; };
	constexpr row rows[]{
		{0xf517,0,0,0,0,0xff80000000ULL,3,0xff80000000ULL,0x7fc00003,0,0,4},
		{0xf517,0,0,0x100,0,0xff80000000ULL,3,0xff80000000ULL,0xffffc00003ULL,0,0,4},
		{0xf474,0,0,0,0,0xff80000800ULL,0,0xff80000,0,0x800,0x800,4},
		{0xf474,0,0,0x100,0,0xff80000800ULL,0,0xfffff80000ULL,0,0x800,0x800,4},
		{0x7722,0xa55a,0,0x100,0,0,0,0,0,0,0,6},
		{0xf537,0,0,0,0,0xff80000000ULL,0,0xff80000000ULL,0xff80400000ULL,0,0,4},
		{0xf537,0,0,0x100,0,0xff80000000ULL,0,0xff80000000ULL,0x400000,0,0,4},
		{0x6184,0xf0,0x800,0x100,0xa0,0,0,0,0,0x1800,0x1800,5},
		{0x6184,0xf0,0x800,0x100,0xa00,0,0,0,0,0x800,0x1800,5}
	};
	std::vector<clocked_register_case> cases;
	for (auto const &v : rows) {
		std::vector<u16> words{0x75f8,0x100,0x124,v.opcode};
		if (v.extension) words.push_back(v.extension);
		words.insert(words.end(),{0x75f8,0x100,0x124,0xf4e1});
		auto initial = initial_case("atomic-shift-carry-bit-test-and-peripheral-literals",{0xf495});
		for (unsigned i = 0; i != words.size(); ++i) initial.program[1+i].value = words[i];
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,v.a},{r::STATE_B,v.b},{r::STATE_AR4,0x120},
			{r::STATE_ST0,v.st0},{r::STATE_ST1,v.st1}});
		initial.data = {{0x100,0x1111},{0x120,v.operand},{0x22,0}};
		auto final = observation("fixed-shift-bit-and-peripheral-results",{{STATE_GENPC,0x601+words.size()},
			{r::STATE_A,v.out_a},{r::STATE_B,v.out_b},{r::STATE_AR4,0x120},{r::STATE_ST0,v.flags,v.mask},{r::STATE_ST1,v.st1}},
			{{0x100,0x1111},{0x120,v.operand},{0x22,u16(v.opcode == 0x7722 ? 0xa55a : 0)}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("before-shift-markers",{{STATE_GENPC,0x601}})},
			{2,observation("shift-markers-running",{})},{48,std::move(final)}},3,48};
		c.pipeline_frontend = false; c.observed_data_space = AS_IO;
		c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1111,0,false},{true,0x124,0x1111,0,false,{},v.interval}};
		cases.push_back(std::move(c));
	}
	auto initial = initial_case("atomic-BACC-captured-target-without-call-frame",{0xf495,0x75f8,0x100,0x124,0xf4e2,0xffff});
	initial.program.insert(initial.program.end(),{{0x700,0x75f8},{0x701,0x101},{0x702,0x124},{0x703,0xf4e1}});
	initial.initial.insert(initial.initial.end(),{{r::STATE_A,0x700},{r::STATE_B,0x605},{r::STATE_SP,0x300},{r::STATE_ST1,0}});
	initial.data = {{0x100,0x1111},{0x101,0x3333},{0x2ff,0xbeef}};
	auto final = observation("BACC-target-unchanged-stack-and-port-time",{{STATE_GENPC,0x704},{r::STATE_SP,0x300},
		{r::STATE_A,0x700},{r::STATE_B,0x605}});
	final.expected_data = initial.data;
	clocked_register_case c{{{0,std::move(initial)},{1,observation("before-BACC-marker",{{STATE_GENPC,0x601}})},
		{2,observation("BACC-running",{})},{48,std::move(final)}},3,48};
	c.pipeline_frontend = false; c.observed_data_space = AS_IO;
	c.data_events = std::vector<clocked_data_event>{{true,0x124,0x1111,0,false},{true,0x124,0x3333,0,false,{},9}};
	cases.push_back(std::move(c));
	return cases;
}
}
