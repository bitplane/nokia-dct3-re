// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for(unsigned start : {2U,81U}){
		auto initial=initial_case("MVDD-Y-AR2-circular-receive-ring-not-AR6",{0xec19,0xe59c,0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_AR0,1},{r::STATE_AR2,0x880+start},{r::STATE_AR3,0x1300},{r::STATE_AR6,0x7777},{r::STATE_BK,82}});
		auto final=observation("26-retained-ring-words-and-both-pointer-updates",{{STATE_GENPC,0x603},{r::STATE_AR2,0x880+(start+26)%82},{r::STATE_AR3,0x131a},{r::STATE_AR6,0x7777},{r::STATE_RC,0}});
		clocked_register_case c{{{0,std::move(initial)},{100,std::move(final)}},12U,100};c.data_events=std::vector<clocked_data_event>{};
		for(unsigned i=0;i<26;++i){u16 const destination=0x880+(start+i)%82,value=0x6000+i;c.states.front().observation.data.push_back({0x1300+i,value});c.states.back().observation.expected_data.push_back({destination,value});
			c.data_events->push_back({false,0x1300+i,value,6+i,true});if(i)c.data_events->push_back({true,0x880+(start+i-1)%82,u16(0x6000+i-1),6+i,true});}
		c.data_events->push_back({true,0x880+(start+25)%82,0x6019,32,true});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
