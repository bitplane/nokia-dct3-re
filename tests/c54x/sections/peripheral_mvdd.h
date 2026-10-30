// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_OBSERVE_MVDD
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (u16 destination : {0x20,0x33,0x5f}) for (u16 value : {0,0x7f,0x8000,0xffff})
	for (unsigned x=0;x<4;++x) for (unsigned y=0;y<4;++y) {
		u32 const base=cases.size()%2 ? 0x20000 : 0;
		u16 const opcode=0xe500|(x<<6)|(y<<2)|1;
		auto initial=initial_case("MVDD-retains-dual-addresses-and-peripheral-wait",{opcode,0xf4e1});
		for (auto &word : initial.program) word.address+=base;
		initial.data={{0x14b2,value},{destination,0xaaaa}};
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x900},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfedcba9876ULL},{r::STATE_AR0,3},
			{r::STATE_AR2,0x14b2},{r::STATE_AR3,destination},{r::STATE_BK,0}});
		auto delta=[](unsigned mode) { return mode==1 ? -1 : mode==2 ? 1 : mode==3 ? 3 : 0; };
		auto final=observation("single-captured-word-and-unchanged-machine-registers",{{r::STATE_AR2,0x2222},{r::STATE_AR3,0x3333},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfedcba9876ULL},{r::STATE_SP,0x1000},
			{r::STATE_ST0,0xa608},{r::STATE_ST1,0x900}},{{destination,value},{0x14b2,0xdead}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("MVDD-running",{})},
			{4,observation("dual-address-modifiers-applied-once",{{r::STATE_AR2,u16(0x14b2+delta(x))},{r::STATE_AR3,u16(destination+delta(y))}})},
			{5,observation("pointers-no-longer-live",{})},{12,std::move(final)}},4+unsigned(cases.size()%4),12};
		c.actions={{4,{{r::STATE_AR2,0x2222},{r::STATE_AR3,0x3333}}},{5,{},false,{},{},{{0x14b2,0xdead}}}};
		c.data_events=std::vector<clocked_data_event>{{false,0x14b2,value,5},{true,destination,value,7}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
