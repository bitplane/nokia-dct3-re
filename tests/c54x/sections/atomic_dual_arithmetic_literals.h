// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	struct row { u16 opcode,x,y,t,st1,ar4,ar5,out4,out5,out_t; u64 a,b,out_a,out_b; };
	constexpr row rows[]{
		{0xe13a,0x55,0xaa,0x4444,0x100,0xf91,0xf90,0xf92,0xf90,0x4444,0x77778888,0x100,0x77cd0888,0x3972},
		{0xa6ab,0x8765,0x1234,8,0x100,0xf90,0xf91,0xf91,0xf92,0x8765,0x1000,0x2222,0x9a0aa84,0x2222},
		{0xa7ab,0x8765,0xfffe,0,0x140,0xf90,0xf91,0xf91,0xf92,0x8765,0x1234,0,0x1234,0xfffffde26cULL}
	};
	std::vector<clocked_register_case> cases;
	for (auto const &v : rows) {
		auto initial = initial_case("LMS-and-MACSU-fixed-dual-memory-examples",{0xf495,v.opcode,0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,v.a},{r::STATE_B,v.b},{r::STATE_T,v.t},
			{r::STATE_ST0,0},{r::STATE_ST1,v.st1},{r::STATE_AR4,v.ar4},{r::STATE_AR5,v.ar5}});
		initial.data = {{0xf90,v.x},{0xf91,v.y}};
		auto final = observation("fixed-product-sign-rounding-and-dual-address-results",{{STATE_GENPC,0x603},
			{r::STATE_A,v.out_a},{r::STATE_B,v.out_b},{r::STATE_T,v.out_t},{r::STATE_ST1,v.st1},
			{r::STATE_AR4,v.out4},{r::STATE_AR5,v.out5}},{{0xf90,v.x},{0xf91,v.y}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-dual-operation-snapshot",{{STATE_GENPC,0x601}})},
			{2,observation("dual-operation-running",{})},{32,std::move(final)}},1,32};
		c.pipeline_frontend = false; c.data_events = std::vector<clocked_data_event>{{false,0xf90,v.x,0,false},{false,0xf91,v.y,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
