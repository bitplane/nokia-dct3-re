// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	struct vector { u64 value; int shift; u32 result[4]; };
	// Explicit SXM/SST right-shift boundaries, independent of CPU helpers.
	constexpr vector vectors[]{
		{0x8000000001ULL,-16,{0x00800000,0xff800000,0x00800000,0xff800000}},
		{0xffffffffffULL,-16,{0x00ffffff,0xffffffff,0x00ffffff,0xffffffff}},
		{0x8000000001ULL,-1,{0,0,0xffffffff,0x80000000}}};
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 120; ++index) {
		auto const &v = vectors[index / 40];
		unsigned const form = (index / 8) % 5;
		bool const source_b = index & 1, extension = form == 2 || form == 3;
		u16 const opcode = form < 2 ? 0x8493 | (form << 9) | (source_b ? 0x100 : 0) :
			extension ? 0x6f93 : 0xe489 | (source_b ? 0x200 : 0);
		u16 const literal = (form == 2 ? 0x0c80 : 0x0c60) | (source_b ? 0x100 : 0) | (v.shift & 31);
		u16 const result = u16(v.result[(index >> 1) & 3] >> (form == 0 || form == 2 ? 0 : 16));
		u16 const st1 = 0x800 | (index & 2 ? 0x100 : 0) | (v.shift & 31);
		auto initial = initial_case("store-shift-boundary", {opcode,u16(extension ? literal : 0xf4e1),0xf4e1});
		initial.data = {{0x600,0xa55a},{0x610,0x1234}};
		initial.initial.insert(initial.initial.end(), {{r::STATE_A,source_b ? 1 : v.value},{r::STATE_B,source_b ? v.value : 1},
			{r::STATE_AR2,0x610},{r::STATE_AR3,0x600},{r::STATE_T,0},{r::STATE_ST0,0x1c00},
			{r::STATE_ST1,st1},{r::STATE_PMST,index & 4 ? 1U : 0U}});
		clocked_register_case c{{{0,std::move(initial)}},index & 1 ? 3U : 1U,24};
		c.pipeline_frontend = false;
		c.states.push_back({24,observation("right-shift-before-saturation", {{r::STATE_A,source_b ? 1 : v.value},
			{r::STATE_B,source_b ? v.value : 1},{r::STATE_AR2,form == 4 ? 0x611U : 0x610U},{r::STATE_AR3,0x601},
			{r::STATE_T,form == 4 ? 0x1234U : 0U},{r::STATE_ST0,0x1c00},{r::STATE_ST1,st1},
			{r::STATE_PMST,index & 4 ? 1U : 0U},{STATE_GENPC,extension ? 0x603U : 0x602U}},{{0x600,result},{0x610,0x1234}})});
		c.data_events = std::vector<clocked_data_event>{};
		if (form == 4) c.data_events->push_back({false,0x610,0x1234,0,false});
		c.data_events->push_back({true,0x600,result,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
