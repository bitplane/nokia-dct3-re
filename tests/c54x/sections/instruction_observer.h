// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_INSTRUCTION_EVENTS
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr bool staged = true;
	for(unsigned form=0;form<3;++form)for(u32 base : {0U,0x20000U})for(bool overlay : {false,true}){
		u32 const physical=overlay ? 0 : base;bool const two=form==2;
		auto initial=initial_case("instruction-observer-is-Execute-not-program-bus",{0xf495});
		for(auto &word : initial.program)word.address+=physical;
		std::vector<u16> code=form ? std::vector<u16>{u16(two ? 0xff45 : 0xfd45),0xe966,u16(two ? 0xe955 : 0xf495),u16(two ? 0xff44 : 0xfd44),0xe977,u16(two ? 0xf495 : 0xf4e1),0xf4e1} : std::vector<u16>{0xec02,0xf495,0xf073,0x0608,0xe966,0xf495,0xf495,0xf495,0xe977,0xf4e1};
		for(unsigned i=0;i<code.size();++i)initial.program[i].value=code[i];
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_PMST,overlay ? 0x20U : 0U},{r::STATE_ST0,0},{r::STATE_ST1,0x900},{r::STATE_A,form ? 1U : 0U},{r::STATE_B,0}});
		auto final=observation("logical-page-repeat-and-XC-annulment",{{r::STATE_A,form ? 1U : 0U},{r::STATE_B,0x77}});
		unsigned const seed=form+staged+(base!=0)+overlay;clocked_register_case c{{{0,std::move(initial)},{40,std::move(final)}},std::array<unsigned,4>{1,3,6,9}[seed%4],40};c.pipeline_frontend=staged;c.data_events=std::vector<clocked_data_event>{};
		c.instruction_events=std::vector<memory_word>{};auto &events=*c.instruction_events;
		if(!form)events={{base+0x600,0xec02},{base+0x601,0xf495},{base+0x601,0xf495},{base+0x601,0xf495},{base+0x602,0xf073},{base+0x608,0xe977},{base+0x609,0xf4e1}};
		else{events.push_back({base+0x600,u16(two ? 0xff45 : 0xfd45)});if(!two)events.push_back({base+0x602,0xf495});events.push_back({base+0x603,u16(two ? 0xff44 : 0xfd44)});events.push_back({base+0x604,0xe977});if(two)events.push_back({base+0x605,0xf495});events.push_back({base+(two ? 0x606 : 0x605),0xf4e1});}
		c.actions={{5,{},false,{{physical+0x600,0}}}};cases.push_back(std::move(c));
	}
	return cases;
}
}
