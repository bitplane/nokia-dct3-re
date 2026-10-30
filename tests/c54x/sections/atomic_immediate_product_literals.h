// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	struct row { u16 opcode,literal,st0,st1,t; u64 a,b,out_a,out_b; u16 out_t,flags,mask; };
	constexpr row rows[]{
		{0x6283,4,0,0x40,0x1234,0,0x5678,24,0x5678,3,0,0},
		{0x6383,0xfffe,0x800,0,3,24,0x123456,24,0xfffffffffaULL,3,0x800,0xffff},
		{0x6383,0xfffe,0x800,0x40,3,24,0x123456,24,0xfffffffff4ULL,3,0x800,0xffff},
		{0x6683,0xfffe,0x800,0,3,100,10,4,10,3,0x800,0xffff},
		{0x6783,0xfffe,0x800,0x40,3,4,10,4,0xfffffffffeULL,3,0x800,0xffff},
		{0x6683,2,0x800,0,3,0x1234,0x7ffffffe,0x80000004,0x7ffffffe,3,0xc00,0xffff},
		{0x6683,2,0x800,0x200,3,0x1234,0x7ffffffe,0x7fffffff,0x7ffffffe,3,0xc00,0xffff},
		{0x6783,2,0x800,0,3,0x1234,0x7ffffffe,0x1234,0x80000004,3,0xa00,0xffff},
		{0x6783,2,0x800,0x200,3,0x1234,0x7ffffffe,0x1234,0x7fffffff,3,0xa00,0xffff},
		{0xf166,0xfffe,0xa00,0,0x2000,0x1234,0,0x1234,0xffffffc000ULL,0x2000,0,0},
		{0x62f8,5,0xa00,0,0,0,0xffffffc000ULL,15,0xffffffc000ULL,3,0,0}
	};
	std::vector<clocked_register_case> cases;
	for (auto const &v : rows) {
		bool const absolute = v.opcode == 0x62f8, memory = v.opcode != 0xf166;
		auto initial = initial_case("immediate-MPY-MAC-destination-and-overflow-literals",{0xf495,v.opcode});
		initial.program[2].value = absolute ? 0x200 : v.literal;
		if (absolute) initial.program[3].value = v.literal;
		initial.program[3 + absolute].value = 0xf4e1;
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,v.a},{r::STATE_B,v.b},{r::STATE_T,v.t},
			{r::STATE_AR3,0x200},{r::STATE_ST0,v.st0},{r::STATE_ST1,v.st1}}); initial.data = {{0x200,3}};
		auto final = observation("literal-product-flags-T-and-source-preservation",{{STATE_GENPC,0x604 + unsigned(absolute)},
			{r::STATE_A,v.out_a},{r::STATE_B,v.out_b},{r::STATE_T,v.out_t},{r::STATE_AR3,0x200},{r::STATE_ST1,v.st1}},{{0x200,3}});
		if (v.mask) final.expected.push_back({r::STATE_ST0,v.flags,v.mask});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-product-snapshot",{{STATE_GENPC,0x601}})},
			{2,observation("product-running",{})},{32,std::move(final)}},1,32};
		c.pipeline_frontend = false; c.data_events = std::vector<clocked_data_event>{};
		if (memory) c.data_events->push_back({false,0x200,3,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
