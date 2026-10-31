// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for(unsigned count : {0U,1U,7U})for(unsigned pattern=0;pattern<4;++pattern)for(unsigned form=0;form<4;++form)
	for(unsigned slot=0;slot<2;++slot)for(unsigned accumulator=0;accumulator<2;++accumulator){
		unsigned const seed=count+pattern+form+slot+accumulator,read=7+slot;u32 const base=seed&1 ? 0x20000 : 0;
		u16 const value=std::array<u16,4>{0,1,0x8000,0xffff}[pattern],mode=std::array<u16,4>{0x12,0x84,0x94,0x8c}[form],status=0x800|((seed%4)<<8)|(seed&2 ? 0x4000 : 0);
		u16 const source=form ? 0x2800 : status&0x4000 ? 0x1e12 : 0x1412,product=u16(s32(s16(value))*3);
		auto initial=initial_case("RPTBD-delay-T-load-feeds-every-body-multiply",{0xf272,0x605});
		for(auto &word : initial.program)word.address+=base;
		initial.program[2+slot].value=0x3000|mode;initial.program[4].value=0x2083|(accumulator<<8);initial.program[5].value=0x8092|(accumulator<<8);initial.program[6].value=0xf4e1;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0x2628},{r::STATE_ST1,status},{r::STATE_A,0x123456789aULL},{r::STATE_B,0x123456789aULL},{r::STATE_SP,0x1e00},{r::STATE_T,0x5555},{r::STATE_BRC,count}});
		for(unsigned i=0;i<8;++i)initial.initial.push_back({r::STATE_AR0+i,i==2 ? 0x4000U : i==3 ? 0x3000U : i==4 ? 0x2800U : 0x3300+i});
		initial.data={{source,value},{0x3000,3}};for(unsigned i=0;i<count+3;++i)initial.data.push_back({0x3fff+i,0xdead});
		auto final=observation("one-load-count-plus-one-products-and-independent-pointers",{{STATE_GENPC,base+0x607},{r::STATE_T,value},{r::STATE_BRC,0},{r::STATE_ST1,status},{r::STATE_SP,0x1e00},{int(accumulator ? r::STATE_A : r::STATE_B),0x123456789aULL}},{{0x3fff,0xdead},{0x4001+count,0xdead}});
		for(unsigned i=0;i<8;++i)final.expected.push_back({int(r::STATE_AR0+i),i==2 ? 0x4001+count : i==3 ? 0x3000U : i==4 ? u16(0x2800+(form==2 ? 1 : form==3 ? -1 : 0)) : 0x3300+i});
		for(unsigned i=0;i<=count;++i)final.expected_data.push_back({0x4000+i,product});
		clocked_register_case c{{{0,std::move(initial)},{80,std::move(final)}},std::array<unsigned,3>{5,read,12}[seed%3],80};
		c.data_events=std::vector<clocked_data_event>{{false,source,value,read,true}};
		for(unsigned i=0;i<=count;++i){c.data_events->push_back({false,0x3000,3,9+2*i,true});c.data_events->push_back({true,0x4000+i,product,11+2*i,true});}
		std::stable_sort(c.data_events->begin(),c.data_events->end(),[](auto const &a,auto const &b){return a.cycle<b.cycle || (a.cycle==b.cycle && !a.write && b.write);});
		c.actions={{read+1,{},false,{{base+0x602+slot,0x3000}},{},{{source,u16(~value)}}}};cases.push_back(std::move(c));
	}
	return cases;
}
}
