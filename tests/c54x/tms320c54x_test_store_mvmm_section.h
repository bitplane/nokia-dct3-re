// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> store_mvmm_cases(bool accumulator)
{
	std::vector<clocked_register_case> cases;
	for(unsigned count : {1U,2U})for(unsigned source=0;source<8;++source)for(unsigned destination=0;destination<8;++destination)
	for(unsigned gap=0;gap<2;++gap)for(unsigned form=0;form<(accumulator ? 4U : 1U);++form)for(unsigned pattern=0;pattern<(accumulator ? 4U : 1U);++pattern){
		unsigned const seed=count+source+destination+gap+form+pattern,width=accumulator ? 1 : 2,move=width*count+gap,read=3+move;
		u32 const base=seed&1 ? 0x20000 : 0;u16 const value=std::array<u16,4>{0,1,0x8000,0xffff}[pattern];
		auto initial=initial_case("store-MVMM-pointer-bypass-and-following-load",{0xf495});
		for(auto &word : initial.program)word.address+=base;
		for(unsigned i=0;i<count;++i){initial.program[width*i].value=(accumulator ? 0x8090|(form<<8) : 0x7690)|source;if(!accumulator)initial.program[2*i+1].value=i ? 0 : 0x3532;}
		initial.program[move].value=0xe700|(source<<4)|destination;initial.program[move+1].value=0x1080|destination;initial.program[move+2].value=0xf4e1;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0},{r::STATE_ST1,0x800},{r::STATE_A,u64(value)<<(form&2 ? 16 : 0)},{r::STATE_B,u64(value)<<(form&2 ? 16 : 0)}});
		for(unsigned i=0;i<8;++i)initial.initial.push_back({r::STATE_AR0+i,i==source ? 0x1200U : 0x1400U});
		for(unsigned i=0;i<3;++i)initial.data.push_back({0x1200+i,0x55aa});
		auto final=observation("both-pointer-registers-and-loaded-value",{{int(r::STATE_AR0+source),0x1200+count},{int(r::STATE_AR0+destination),0x1200+count},{r::STATE_A,0x55aa}});
		for(unsigned i=0;i<count;++i)final.expected_data.push_back({0x1200+i,u16(accumulator ? value : i ? 0 : 0x3532)});
		clocked_register_case c{{{0,std::move(initial)},{20,std::move(final)}},read-1+seed%3,20};c.clear_pipeline_before_restore=accumulator;
		c.data_events=std::vector<clocked_data_event>{};
		for(unsigned i=0;i<count;++i)c.data_events->push_back({true,0x1200+i,u16(accumulator ? value : i ? 0 : 0x3532),accumulator ? 6+i : 7+2*i,true});
		if(!accumulator || gap)c.data_events->push_back({false,0x10+source,u16(0x1200+count),read,true});
		c.data_events->push_back({true,0x10+destination,u16(0x1200+count),read+1,true});
		c.data_events->push_back({false,0x1200+count,0x55aa,read+3,true});
		std::stable_sort(c.data_events->begin(),c.data_events->end(),[](auto const &a,auto const &b){return a.cycle<b.cycle;});cases.push_back(std::move(c));
	}
	return cases;
}
}
