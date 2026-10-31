// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for(bool body : {false,true})for(u32 base : {0U,0x20000U})for(u16 value : {u16(0x1400),u16(0x5aa5)})for(unsigned count : {0U,3U})for(bool long_count : {false,true}){
		auto initial=initial_case("mapped-move-ownership-belongs-to-repeat-body",{0xf495});
		for(auto &word : initial.program)word.address+=base;
		u32 cursor=base+0x600;
		auto emit=[&](u16 word){initial.program.push_back({cursor++,word});};
		auto move=[&](){emit(0x710f);emit(0x12);};
		if(!body)move();
		emit(long_count ? 0xf070 : 0xec00|count);if(long_count)emit(count);
		if(body)move();else emit(0xf495);emit(0xf4e1);
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0},{r::STATE_ST1,0x800},{r::STATE_TRN,value},{r::STATE_AR2,0x3333}});
		if(body)initial.expected.clear();
		auto final=observation("older-move-survives-independent-repeat-header",{{r::STATE_AR2,body ? 0x3333U : value},{r::STATE_TRN,value},{r::STATE_ST0,0},{r::STATE_ST1,0x800}});
		if(body)final.expected.back().value=1;
		else final.expected.push_back({STATE_GENPC,cursor});
		clocked_register_case c{{{0,std::move(initial)},{40,std::move(final)}},(count+body+long_count+unsigned(base!=0))&1 ? 7U : 4U,40};
		c.require_idle=!body;
		c.clear_pipeline_before_restore=true;
		c.data_events=std::vector<clocked_data_event>{};
		if(!body){c.data_events->push_back({false,0xf,value,0,false});c.data_events->push_back({true,0x12,value,0,false});}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
