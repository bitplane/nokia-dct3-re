// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned vector = 0; vector != 32; ++vector) for (unsigned masked = 0; masked != 2; ++masked) {
		u32 const base = (vector+masked)%2 ? 0x20000 : 0, handler = base+0x100+vector*4;
		u16 const opcode = 0xf7c0 | vector;
		auto initial = initial_case("staged-INTR-retained-vector-and-RET-continuation",{opcode,0x890f,0xf4e1});
		for (auto &word : initial.program) word.address += base;
		initial.program.insert(initial.program.end(),{{handler,0x880e},{handler+1,0xfc00}});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_PMST,0x100},{r::STATE_ST0,0xa608},
			{r::STATE_ST1,masked ? 0x900U : 0x100U},{r::STATE_IFR,0xffff},{r::STATE_IMR,0},{r::STATE_SP,0x3000},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfedcba9876ULL},{r::STATE_T,0x1111},{r::STATE_TRN,0x2222}});
		auto final = observation("one-frame-handler-and-continuation-after-poison",{{STATE_GENPC,base+0x603},{r::STATE_PMST,0x100},
			{r::STATE_IMR,0},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x900},{r::STATE_SP,0x3000},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfedcba9876ULL},{r::STATE_T,0x789a},{r::STATE_TRN,0x9876},
			{r::STATE_IFR,vector < 16 ? 0xffffU : 0xffffU ^ (1U << (vector-16))}},{{0x2fff,0xdead}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("INTR-running",{})},
			{3,observation("RTN-is-Decode-owned",{{r::STATE_RTN,0x601}})},{4,observation("entry-and-return-running",{})},
			{32,std::move(final)}},(vector+masked)%2 ? 5U : 3U,32};
		c.actions = {{3,{},false,{{base+0x600,0xf495}}},{5,{},false,{{handler,0xf495}}},
			{9,{},false,{}, {},{{0x2fff,0xdead}}}};
		// RET's Read precedes the older STLM Execute in the shared model clock.
		c.data_events = std::vector<clocked_data_event>{{true,0x2fff,0x601,6},{false,0x2fff,0x601,9},
			{true,0xe,0x789a,9},{true,0xf,0x9876,15}};
		c.program_events = {{false,base+0x600,opcode,0,false},{false,handler,0x880e,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
