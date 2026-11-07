// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (bool b : {false,true}) for (u16 address : {0x5000U,0xf495U,0xfffeU})
	for (unsigned snapshot : {3U,6U,9U,10U}) {
		u64 const a=0x123456789aULL, other=0xffabcdef01ULL;
		u32 const value=u32(b ? other : a);
		u16 const low_address=address^1; // Dmem pairs preserve the instruction's high-word address.
		auto initial=initial_case("absolute-DST-in-RETD-two-word-delay",{0xfe00,u16(b ? 0x4ff8 : 0x4ef8),address,0xf495});
		initial.program.push_back({0x700,0xf4e1});
		initial.data={{0x3000,0x700},{address,0xdead},{low_address,0xbeef}};
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,a},{r::STATE_B,other},
			{r::STATE_SP,0x3000},{r::STATE_ST0,0xaa5},{r::STATE_ST1,0x100}});
		auto final=observation("both-halves-before-return-target",{{STATE_GENPC,0x701},
			{r::STATE_A,a},{r::STATE_B,other},{r::STATE_SP,0x3001},
			{r::STATE_ST0,0xaa5},{r::STATE_ST1,0x100}},{{address,u16(value>>16)},{low_address,u16(value)}});
		clocked_register_case c{{{0,std::move(initial)},{24,std::move(final)}},snapshot,24};
		c.data_events=std::vector<clocked_data_event>{{false,0x3000,0x700,0,false},
			{true,address,u16(value>>16),0,false},{true,low_address,u16(value),0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
