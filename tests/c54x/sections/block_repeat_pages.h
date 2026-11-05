// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_HIGH_PROGRAM
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (bool delayed : {false,true}) for (bool hazard : {false,true})
	for (unsigned count : {0U,1U,2U,7U}) for (unsigned pair=0;pair<3;++pair) {
		u32 const caller=std::array<u32,3>{0,0x20000,0x7f0000}[pair], target=std::array<u32,3>{0x20000,0x7f0000,0}[pair];
		u16 const return_pc=delayed ? 0x607 : 0x605;
		unsigned const calls=hazard ? 1 : count+1;
		auto initial=initial_case("block-repeat-low-PC-comparator-across-foreign-page",{0xf072,0x609});initial.program.clear();
		for (u32 page : {caller,target}) for(unsigned i=0;i<32;++i)initial.program.push_back({page+0x600+i,0xf495});
		initial.program.insert(initial.program.end(),{{caller+0x600,0xf072},{caller+0x601,0x609},{caller+0x602,0x6d94},
			{caller+0x603,u16((delayed ? 0xfb80 : 0xf980)|(target>>16))},{caller+0x604,u16(hazard ? 0x608 : 0x610)},
			{caller+0x60a,0xf4e1},{target+0x602,0x6d95},{target+0x609,0x6d96},{target+0x60a,0xf4e4},{target+0x610,0x6d96},{target+0x611,0xf4e4}});
		if(delayed)initial.program.push_back({caller+0x605,0x6d97});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,caller+0x600},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x900},
			{r::STATE_SP,0x3000},{r::STATE_BRC,count},{r::STATE_AR4,0x1400},{r::STATE_AR5,0x1500},{r::STATE_AR6,0x1600},{r::STATE_AR7,0x1700}});
		auto final=observation("foreign-endpoint-consumes-current-page-loop",{{STATE_GENPC,caller+0x60b},{r::STATE_SP,0x3000},
			{r::STATE_AR4,0x1400+calls},{r::STATE_AR5,0x1500+(hazard ? count : 0)},{r::STATE_AR6,0x1601+count},
			{r::STATE_AR7,0x1700+(delayed ? calls : 0)},{r::STATE_BRC,0},{r::STATE_RSA,0x602},{r::STATE_REA,0x609},
			{r::STATE_ST0,0xa608},{r::STATE_ST1,0x900}});
		clocked_register_case c{{{0,std::move(initial)},{240,std::move(final)}},
			std::array<unsigned,4>{6,10,16,24}[(count+pair+1U+unsigned(hazard))%4],240};
		c.data_events=std::vector<clocked_data_event>{};
		for(unsigned i=0;i<calls;++i) {
			c.data_events->push_back({true,0x2fff,return_pc,0,false});c.data_events->push_back({true,0x2ffe,u16(caller>>16),0,false});
			c.data_events->push_back({false,0x2ffe,u16(caller>>16),0,false});c.data_events->push_back({false,0x2fff,return_pc,0,false});
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
