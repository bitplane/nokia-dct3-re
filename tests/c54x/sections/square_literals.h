// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	struct example { char const *name; u16 opcode,word,st0,st1,t; u64 a,b,result_a,result_b; u16 result_t,mask,flags; };
	// SPRU172C SQUR/SQURA/SQURS examples: literal independent outcomes.
	constexpr example examples[]{
		{"SQUR fractional saturation",0x2682,0x8000,0,0x240,0,0,0,0x7fffffff,0,0x8000,0x400,0x400},
		{"SQUR absolute B",0x27f8,0xfffe,0,0x100,0,0x1234,0,0x1234,4,0xfffe,0,0},
		{"SQUR register A",0xf48d,0,0,0x100,0xbeef,0xfffffe0000ULL,0x1234,4,0x1234,0xbeef,0,0},
		{"SQUR register B saturation",0xf58d,0,0,0x240,0xbeef,0xff80000000ULL,0,0xff80000000ULL,0x7fffffff,0xbeef,0x200,0x200},
		{"SQURA preserves carry",0x3882,0xfffe,0x800,0x100,0,10,0x1234,14,0x1234,0xfffe,0x800,0x800},
		{"SQURS absolute B",0x3bf8,3,0,0x100,0,0x1234,5,0x1234,0xfffffffffcULL,3,0,0}
	};
	std::vector<clocked_register_case> cases;
	for (auto const &v : examples) {
		bool const absolute = (v.opcode & 0xff) == 0xf8, memory = v.opcode < 0x4000;
		auto initial = initial_case(v.name,{v.opcode,absolute ? u16(0x5000) : u16(0xf4e1),0xf4e1});
		// One leading NOP gives the section runner a pre-instruction snapshot.
		initial.program.push_back({0x5ff,0xf495});
		initial.data = {{0x5000,v.word}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_A,v.a},{r::STATE_B,v.b},
			{r::STATE_T,v.t},{r::STATE_ST0,v.st0},{r::STATE_ST1,v.st1},{r::STATE_AR2,0x5000}});
		auto final = observation(v.name, {{STATE_GENPC,absolute ? 0x603U : 0x602U},{r::STATE_A,v.result_a},
			{r::STATE_B,v.result_b},{r::STATE_T,v.result_t},{r::STATE_AR2,0x5000},{r::STATE_ST1,v.st1}},{{0x5000,v.word}});
		if (v.mask) final.expected.push_back({r::STATE_ST0,v.flags,v.mask});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("before-square", {{r::STATE_A,v.a},{r::STATE_B,v.b}})},
			{2,observation("square-running", {})},{32,std::move(final)}},1,32};
		c.pipeline_frontend = false;
		c.data_events = std::vector<clocked_data_event>{};
		if (memory) c.data_events->push_back({false,0x5000,v.word,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
