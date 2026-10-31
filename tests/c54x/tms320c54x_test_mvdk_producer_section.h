// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> mvdk_producer_cases(bool accumulator)
{
	std::vector<clocked_register_case> cases;
	for(unsigned lane=0;lane<6;++lane)for(unsigned form=0;form<3;++form)for(unsigned producer=0;producer<3;++producer)
	for(unsigned prefix : {1U,2U})for(unsigned dest=0;dest<(accumulator ? 6U : 2U);++dest)for(unsigned pattern=0;pattern<4;++pattern){
		unsigned const seed=lane+form+producer+prefix+dest+pattern,access=5+prefix+unsigned(form==2);
		u32 const base=seed&1 ? 0x20000 : 0;u16 const source=8+lane,destination=accumulator ? 8+dest : dest ? 0x6000 : 0x13;
		u16 const value=std::array<u16,4>{0,1,0x8000,0xffff}[pattern];bool const b=lane>=3;unsigned const part=lane%3;
		u64 const original=b ? 0xfedcba9876ULL : 0x123456789aULL;
		// Adjacent short LD deliberately observes the implementation's old Access value.
		// A NOP or absolute-source extension supplies the documented separation.
		u16 const captured=producer==2 ? (access==6 ? u16(original>>(16*part)) : part ? 0 : u8(value)) : part==2 ? u8(value) : value;
		u16 const first=producer==0 ? (b ? 0x8000 : 0x8100)|source : producer==1 ? 0x8a00|source : (b ? 0xe900 : 0xe800)|u8(value);
		u16 const move=form==0 ? 0x7100|source : form==1 ? 0x7186 : 0x71f8;
		auto initial=initial_case(producer==2 && access==6 ? "adjacent-short-LD-MVDK-implementation-projection" : "mapped-producer-MVDK-no-conflict-ordering",{first});
		for(auto &word : initial.program)word.address+=base;
		auto put=[&](unsigned offset,u16 word){initial.program.at(offset).value=word;};
		put(prefix,move);put(prefix+1,form==2 ? source : destination);if(form==2)put(prefix+2,destination);put(prefix+(form==2 ? 3 : 2),0xf4e1);
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0xa600},{r::STATE_ST1,0x900},{r::STATE_SP,0x3000},
			{r::STATE_A,producer==0 && b ? value : 0x123456789aULL},{r::STATE_B,producer==0 && !b ? value : 0xfedcba9876ULL},{r::STATE_AR3,0x1403},{r::STATE_AR6,source}});
		initial.data.push_back({0x3000,value});
		auto final=observation("captured-source-final-destination-flags-and-stack-balance",{{STATE_GENPC,base+0x600+prefix+(form==2 ? 4 : 3)},
			{r::STATE_ST0,0xa600},{r::STATE_ST1,0x900},{r::STATE_SP,0x3000+unsigned(producer==1)}},{{destination,u16(captured&(destination==0xa || destination==0xd ? 0xff : 0xffff))}});
		clocked_register_case c{{{0,std::move(initial)},{24,std::move(final)}},access-1+seed%3,24};
		c.data_events=std::vector<clocked_data_event>{};
		if(producer==1)c.data_events->push_back({false,0x3000,value,5,true});
		if(producer!=2)c.data_events->push_back({true,source,value,6,true});
		c.data_events->push_back({false,source,captured,access,true});
		c.data_events->push_back({true,destination,captured,access+unsigned(destination<0x10 || destination>=0x60),true});
		c.program_events={{false,base+0x601+prefix,u16(form==2 ? source : destination),0,false}};
		if(form==2)c.program_events.push_back({false,base+0x602+prefix,destination,0,false});
		c.actions.push_back({3+prefix,{},false,{{base+0x601+prefix,0xdead}}});
		if(form==2)c.actions.push_back({4+prefix,{},false,{{base+0x602+prefix,0xdead}}});
		c.actions.push_back({access,{},false,{},{},{{source,0xdead}}});cases.push_back(std::move(c));
	}
	return cases;
}
}
