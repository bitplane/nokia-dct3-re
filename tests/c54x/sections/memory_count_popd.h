// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for(unsigned count : {0U,1U,7U,15U}){
		u32 const base=count&1 ? 0x20000 : 0;
		auto initial=initial_case("memory-count-POPD-captures-count-once",{0x47f8,0x100,0x8b97,0xf4e1});
		for(auto &word : initial.program)word.address+=base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_SP,0x3000},{r::STATE_AR7,0x4000}});initial.data.insert(initial.data.end(),{{0x100,u16(count)},{0x3fff,0xa55a},{0x4001+count,0xa55a}});
		for(unsigned i=0;i<=count;++i)initial.data.push_back({0x3000+i,u16(0x2468+i)});
		auto final=observation("count-plus-one-stack-transfers",{{r::STATE_SP,0x3001+count},{r::STATE_AR7,0x4001+count},{STATE_GENPC,base+0x604},{r::STATE_RC,0},{r::STATE_ST0,0xe},{r::STATE_ST1,0x800}},{{0x100,0xffff},{0x3fff,0xa55a},{0x4001+count,0xa55a}});
		for(unsigned i=0;i<=count;++i)final.expected_data.push_back({0x4000+i,u16(0x2468+i)});
		clocked_register_case c{{{0,std::move(initial)},{40,std::move(final)}},std::array<unsigned,4>{3,6,9,10}[cases.size()],40};
		c.program_events={{false,base+0x601,0x100,0,false},{false,base+0x602,0x8b97,0,false}};
		c.actions={{3,{},false,{{base+0x601,0xdead}}},{6,{},false,{},{},{{0x100,0xffff}}}};
		for(unsigned i=0;i<=count;++i)c.actions.push_back({i ? 10+i : 9,{},false,{},{},{{0x3000+i,0xdead}}});
		c.data_events=std::vector<clocked_data_event>{{false,0x100,u16(count),6,true}};
		c.data_events->push_back({false,0x3000,0x2468,9,true});c.data_events->push_back({true,0x4000,0x2468,10,true});
		for(unsigned i=1;i<=count;++i){c.data_events->push_back({false,0x3000+i,u16(0x2468+i),10+i,true});if(i>1)c.data_events->push_back({true,0x4000+i-1,u16(0x2468+i-1),10+i,true});}
		if(count)c.data_events->push_back({true,0x4000+count,u16(0x2468+count),11+count,true});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
