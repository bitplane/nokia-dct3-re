// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_CONTEXT_POP_POISON
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for(unsigned count : {0U,1U,2U,7U})for(bool b : {false,true})for(bool equal : {false,true})for(bool zero : {false,true})
	for(bool endpoint : {false,true})for(bool long_delay : {false,true})for(u32 base : {0U,0x20000U}){
		bool const taken=equal==zero;unsigned const iterations=count+1;u16 const end=endpoint ? 0x609 : 0x60b;
		auto initial=initial_case("block-conditional-call-retains-predicate-across-delay",{0xf272,end});
		for(auto &word : initial.program)word.address+=base;
		for(unsigned i=0;i<4;++i)initial.program.push_back({base+0x700+i,0xf495});
		initial.program.insert(initial.program.end(),{{base+0x604,u16((b ? 0xe900 : 0xe800)|unsigned(!zero))},{base+0x605,0xe589},
			{base+0x606,u16((equal ? 0xfb45 : 0xfb44)+(b ? 8 : 0))},{base+0x607,0x700},
			{base+0x608,u16(long_delay ? (b ? 0xf120 : 0xf020) : (b ? 0xe900 : 0xe800)|unsigned(zero))},{base+0x609,u16(long_delay ? unsigned(zero) : 0x6d95)},
			{base+end+1,0x6d97},{base+end+2,0xf4e1},{base+0x700,0x6d96},{base+0x701,0xfc00}});
		if(!endpoint)initial.program.push_back({base+0x60a,0x6d94});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x900},{r::STATE_BRC,count},{r::STATE_SP,0x3000},{r::STATE_RTN,0x5555},
			{r::STATE_A,b ? 0x123456789aULL : 0},{r::STATE_B,b ? 0 : 0xfedcba9876ULL},{r::STATE_AR2,0x5000},{r::STATE_AR3,0x6000}});
		for(unsigned ar=4;ar<8;++ar)initial.initial.push_back({r::STATE_AR0+ar,0x1000+ar*0x100});
		auto final=observation("taken-call-frame-per-iteration-with-single-post-loop-sentinel",{{STATE_GENPC,base+end+3},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x900},{r::STATE_SP,0x3000},
			{r::STATE_BRC,0},{r::STATE_RSA,0x604},{r::STATE_REA,end},{r::STATE_A,b ? 0x123456789aULL : u64(zero)},{r::STATE_B,b ? u64(zero) : 0xfedcba9876ULL},
			{r::STATE_AR2,0x5000+iterations},{r::STATE_AR3,0x6000+iterations},{r::STATE_AR4,0x1400+(endpoint ? 0 : iterations)},
			{r::STATE_AR5,0x1500+(long_delay ? 0 : iterations)},{r::STATE_AR6,0x1600+(taken ? iterations : 0)},{r::STATE_AR7,0x1701}});
		for(unsigned i=0;i<=iterations;++i){initial.data.insert(initial.data.end(),{{0x5000+i,u16(0x1357+i*0x2345)},{0x6000+i,0xdead}});final.expected_data.push_back({0x6000+i,u16(i<iterations ? 0x1357+i*0x2345 : 0xdead)});}
		clocked_register_case c{{{0,std::move(initial)},{400,std::move(final)}},std::array<unsigned,4>{4,12,13,18}[(count+b+equal+zero+endpoint+long_delay+unsigned(base!=0))%4],400};
		c.data_events=std::vector<clocked_data_event>{};
		for(unsigned i=0;i<iterations;++i){
			c.data_events->push_back({false,0x5000+i,u16(0x1357+i*0x2345),0,false});c.data_events->push_back({true,0x6000+i,u16(0x1357+i*0x2345),0,false});
			c.program_events.push_back({false,base+0x607,0x700,0,false});
			if(taken){c.data_events->push_back({true,0x2fff,0x60a,0,false});c.data_events->push_back({false,0x2fff,0x60a,0,false});c.program_events.push_back({false,base+0x700,0x6d96,0,false});}
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
