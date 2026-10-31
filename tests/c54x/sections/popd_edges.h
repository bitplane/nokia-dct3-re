// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for(unsigned variant=0;variant<31;++variant){
		if(variant==12)continue; // Legacy memory-count exclusion predates counted-body admission.
		bool const conflict=variant>=15,active=variant>=8 && variant<12;unsigned const index=conflict ? variant-15 : 0;
		u32 const base=conflict && index>=8 ? 0x20000 : 0;
		u16 const destination=conflict ? std::array<u16,4>{0x10,0x12,0x17,0x19}[index%4] : variant<8 ? std::array<u16,8>{0,1,6,7,0x18,0x1e,0x24,0x58}[variant] : active ? 0x1a+variant-8 : 0x4000;
		u16 const writer=(index/4)%2 ? destination : destination==0x12 ? 0x11 : 0x12;
		unsigned const fail=active ? 8 : variant==13 ? 5 : variant==14 ? 3 : 4,finish=conflict ? 12 : fail;
		auto initial=initial_case(conflict ? "POPD-defers-destination-bank-behind-older-store" : "POPD-unsupported-contract-rejects-before-consumption",{u16(conflict ? 0x8000|writer : active ? 0xf072 : variant==14 ? 0x8ba0 : 0x8b97)});
		for(auto &word : initial.program)word.address+=base;
		if(conflict){initial.program[1].value=0x8b97;initial.program[2].value=0xf4e1;}
		if(active){initial.program[1].value=0x603;initial.program[2].value=0x8b97;}
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0xa600},{r::STATE_ST1,0x900},{r::STATE_SP,variant==13 ? 0xeU : 0x3000U},
			{r::STATE_AR7,destination},{r::STATE_A,0x1357},{r::STATE_BRC,1}});
		initial.data.push_back({0x3000,0x2468});if(conflict){initial.data.push_back({destination,0x2222});if(destination!=0x17)initial.initial.push_back({destination==0x19 ? r::STATE_BK : r::STATE_AR0+destination-0x10,0x2222});}
		if(!conflict)initial.expected.clear();
		auto final=observation("one-retained-source-and-destination-or-no-rejected-effects",{{r::STATE_SP,conflict ? 0x3001U : variant==13 ? 0xeU : 0x3000U},
			{r::STATE_AR7,conflict ? destination==0x17 ? 0x2468U : 0x3333U : variant==13 ? 0x4001U : destination}});
		if(conflict){final.expected.push_back({STATE_GENPC,base+0x603});final.expected_data.push_back({destination,0x2468});if(writer!=destination)final.expected_data.push_back({writer,0x1357});}
		else final.expected.back().value=1;
		clocked_register_case c{{{0,std::move(initial)},{finish,std::move(final)}},conflict ? 6U : fail-1,finish};c.require_idle=conflict;
		c.data_events=std::vector<clocked_data_event>{};
		if(conflict){c.data_events->push_back({false,0x3000,0x2468,6,true});c.data_events->push_back({true,writer,0x1357,6,true});c.data_events->push_back({true,destination,0x2468,7,true});
			c.actions.push_back({5,{{r::STATE_AR7,0x3333}}});c.actions.push_back({6,{},false,{},{},{{0x3000,0xdead}}});}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
