// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> mapped_mvdk_cases(unsigned kind)
{
	std::vector<clocked_register_case> cases;
	bool const clock=kind==2,accumulator=kind==1;
	for(unsigned source_index=0;source_index<(clock ? 1U : accumulator ? 19U : 18U);++source_index)
	for(unsigned destination_index=0;destination_index<(clock ? 1U : accumulator ? 6U : 11U);++destination_index)
	for(unsigned form=0;form<3;++form)for(unsigned pattern=0;pattern<4;++pattern){
		unsigned const seed=source_index+destination_index+form+pattern;
		u32 const base=seed&1 ? 0x20000 : 0;
		u16 const source=clock ? 0x1420 : source_index==18 ? 0x62 : 8+source_index;
		u16 const destination=clock ? 0x58 : accumulator ? 8+destination_index : destination_index<8 ? 0x10+destination_index : destination_index<10 ? 0xe + destination_index-8 : 0x6000;
		u16 value=(clock ? std::array<u16,4>{0,0x3146,0x8006,0xffff} : std::array<u16,4>{0,1,0x8000,0xffff})[pattern];
		if(source==0xa || source==0xd)value&=0xff;
		if(form==1 && source==0x16)value=source;
		u16 const opcode=form==0 ? 0x7100|(source&0x7f) : form==1 ? 0x7196 : 0x71f8;
		unsigned const access=form==2 ? 6 : 5,publication=access+(clock ? 2 : unsigned(destination<0x10 || destination>=0x60));
		auto initial=initial_case("mapped-MVDK-captured-source-and-destination-owned-publication",{opcode,u16(form==2 ? source : destination),u16(form==2 ? destination : 0xf4e1),0xf4e1});
		for(auto &word : initial.program)word.address+=base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,clock ? 0xa628U : 0xa600U},{r::STATE_ST1,0x900},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfedcba9876ULL},{r::STATE_SP,0x3000},{r::STATE_BK,13}});
		for(unsigned i=0;i<8;++i)initial.initial.push_back({r::STATE_AR0+i,0x1400+i});
		initial.data.push_back({source,value});
		if(source>=8 && source<=0xd){
			unsigned const part=(source-8)%3,shift=part*16;u64 const mask=(part==2 ? 0xffULL : 0xffffULL)<<shift;
			u64 const original=source>=0xb ? 0xfedcba9876ULL : 0x123456789aULL;
			initial.initial.push_back({source>=0xb ? r::STATE_B : r::STATE_A,(original&~mask)|((u64(value)<<shift)&mask)});
		}else if(source<0x1a){
			int const reg=source==0xe ? r::STATE_T : source==0xf ? r::STATE_TRN : source==0x18 ? r::STATE_SP : source==0x19 ? r::STATE_BK : r::STATE_AR0+source-0x10;
			initial.initial.push_back({reg,value});
		}
		if(form==1)initial.initial.push_back({r::STATE_AR6,source});
		if(clock){initial.data.push_back({0x58,0});initial.data.push_back({0x58,0});}
		auto final=observation("retained-word-flags-continuation-and-unselected-accumulator-fields",{{STATE_GENPC,base+(form==2 ? 0x604 : 0x603)},
			{r::STATE_ST0,clock ? 0xa628U : 0xa600U},{r::STATE_ST1,0x900}},{{destination,u16(clock ? (value&~u16(1))|BIT(value,1) : value&(destination==0xa || destination==0xd ? 0xff : 0xffff))}});
		if(accumulator){
			u64 a=0x123456789aULL,b=0xfedcba9876ULL;
			auto field=[&](unsigned address,u16 word){if(address<8 || address>0xd)return;unsigned const part=(address-8)%3,shift=part*16;u64 const mask=(part==2 ? 0xffULL : 0xffffULL)<<shift;u64 &target=address>=0xb ? b : a;target=(target&~mask)|((u64(word)<<shift)&mask);};
			field(source,value);if(source!=destination)field(source,0xdead);field(destination,value);
			final.expected.insert(final.expected.end(),{{r::STATE_A,a},{r::STATE_B,b}});
		}
		clocked_register_case c{{{0,std::move(initial)},{20,std::move(final)}},unsigned(int(access)+(clock ? std::array<int,3>{-1,1,3}[seed%3] : int(seed%3)-1)),20};
		c.data_events=std::vector<clocked_data_event>{{false,source,value,access,true},{true,destination,value,publication,true}};
		c.program_events={{false,base+0x601,u16(form==2 ? source : destination),0,false}};
		if(form==2)c.program_events.push_back({false,base+0x602,destination,0,false});
		c.actions.push_back({3,{},false,{{base+0x601,0xdead}}});
		if(form==2)c.actions.push_back({4,{},false,{{base+0x602,0xdead}}});
		if(source!=destination)c.actions.push_back({access,{},false,{},{},{{source,0xdead}}});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
