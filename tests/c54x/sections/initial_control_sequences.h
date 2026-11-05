// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for(unsigned variant=0;variant<3;++variant){
		auto initial=initial_case("independent-initial-control-and-repeat-sequence",{0xf495});
		std::vector<u16> code=variant==0 ? std::vector<u16>{0xf074,0x700,0xec02,0xe598,0xf4e1} : variant==1 ? std::vector<u16>{0xf072,0x603,0xf074,0x700,0xf4e1} : std::vector<u16>{0xec02,0xe5c9,0xe726,0xf4e1};
		for(unsigned i=0;i<code.size();++i)initial.program[i].value=code[i];
		if(!variant)initial.program.insert(initial.program.end(),{{0x700,0x7680},{0x701,0xbeef},{0x702,0xfc00}});
		if(variant==1)initial.program.insert(initial.program.end(),{{0x700,0x1092},{0x701,0x8094},{0x702,0xfc00}});
		initial.initial.insert(initial.initial.end(),{{r::STATE_ST0,0},{r::STATE_SP,0x3000},{r::STATE_AR0,variant==2 ? 1U : 0x900U},{r::STATE_AR2,variant==0 ? 0x400U : variant==1 ? 0x500U : 0x802U},
			{r::STATE_AR3,variant==2 ? 0x900U : 0x500U},{r::STATE_AR4,0xa00},{r::STATE_BRC,variant==1 ? 2U : 0U},{r::STATE_BK,variant==2 ? 3U : 0U}});
		if(variant==2)initial.data={{0x800,0xaaaa},{0x801,0xbbbb},{0x802,0xcccc}};else initial.data={{0x500,0x1111},{0x501,0x2222},{0x502,0x3333}};
		auto final=observation("balanced-calls-terminal-repeat-and-exact-copies",{{STATE_GENPC,0x600+code.size()},{r::STATE_SP,0x3000},{r::STATE_BRC,0},{r::STATE_ST1,0,0x8000}});
		if(!variant)final.expected_data.push_back({0x900,0xbeef});
		if(variant==2)final.expected.push_back({r::STATE_AR6,0x802});
		clocked_register_case c{{{0,std::move(initial)},{128,std::move(final)}},5+variant,128};c.data_events=std::vector<clocked_data_event>{};
		if(!variant)c.data_events->insert(c.data_events->end(),{{true,0x2fff,0x602,0,false},{false,0x2fff,0x602,0,false},{true,0x900,0xbeef,0,false}});
		for(unsigned i=0;i<3;++i){u16 const source=variant==2 ? std::array<u16,3>{0x802,0x800,0x801}[i] : 0x500+i,destination=(variant==0 ? 0x400 : variant==1 ? 0xa00 : 0x900)+i,value=variant==2 ? std::array<u16,3>{0xcccc,0xaaaa,0xbbbb}[i] : 0x1111*(i+1);
			if(variant==1)c.data_events->push_back({true,0x2fff,0x604,0,false});
			c.data_events->push_back({false,source,value,0,false});
			if(variant==1)c.data_events->insert(c.data_events->end(),{{false,0x2fff,0x604,0,false},{true,destination,value,0,false}});
			else if(i)c.data_events->push_back({true,u32(destination-1),u16(variant==2 ? std::array<u16,3>{0xcccc,0xaaaa,0xbbbb}[i-1] : 0x1111*i),0,false});
			c.states.back().observation.expected_data.push_back({destination,value});}
		if(variant!=1)c.data_events->push_back({true,variant==0 ? 0x402U : 0x902U,u16(variant==0 ? 0x3333 : 0xbbbb),0,false});
		if(variant==2)c.data_events->insert(c.data_events->end(),{{false,0x12,0x802,0,false},{true,0x16,0x802,0,false}});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
