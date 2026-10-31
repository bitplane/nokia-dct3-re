// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> conditional_call_cases(bool tc)
{
	std::vector<clocked_register_case> cases;
	for(unsigned condition=0;condition<(tc ? 2U : 6U);++condition)for(unsigned flag=0;flag<(tc ? 2U : 1U);++flag)
	for(unsigned pattern=0;pattern<8;++pattern)for(bool b : {false,true})for(unsigned layout=0;layout<2;++layout)for(u32 base : {0U,0x20000U}){
		u64 const value=std::array<u64,8>{0,1,0x8000000000ULL,0xffffffffffULL,0x7fffffffffULL,0x0100000000ULL,0x0080000000ULL,0x00ffffffffULL}[pattern];
		s64 const signed_value=value&0x8000000000ULL ? s64(value)-0x10000000000LL : s64(value);
		bool const taken=tc ? condition==flag : std::array<bool,6>{signed_value>=0,signed_value<0,signed_value!=0,signed_value==0,signed_value>0,signed_value<=0}[condition];
		u16 const literal=taken ? std::array<u16,6>{0xffff,0,0,1,0,1}[condition] : std::array<u16,6>{0,0xffff,1,0,1,0}[condition];
		u64 const changed=tc ? value : layout ? u64(s64(s16(literal)))&0xffffffffffULL : value^0xffffffffffULL;
		u16 const delay0=tc ? (!layout ? (flag ? 0xf4bc : 0xf5bc) : 0x6d95) : layout ? (b ? 0xf120 : 0xf020) : (b ? 0xf793 : 0xf493);
		u16 const delay1=tc ? (layout ? (flag ? 0xf4bc : 0xf5bc) : 0x6d95) : layout ? literal : 0x6d95;
		auto initial=initial_case("delayed-conditional-call-retains-original-predicate",{u16(tc ? 0xfb20+(condition ? 0x10 : 0) : 0xfb42+condition+(b ? 8 : 0)),0x700,delay0,delay1,0x6d94,0xf4e1});
		for(auto &word : initial.program)word.address+=base;
		initial.program.insert(initial.program.end(),{{base+0x700,0x6d96},{base+0x701,0xfc00}});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,u64(0xa608|(tc && flag ? 0x1000 : 0))},{r::STATE_ST1,0x900},
			{r::STATE_SP,0x3000},{r::STATE_RTN,0x5555},{r::STATE_A,b ? 0x123456789aULL : value},{r::STATE_B,b ? value : 0xfedcba9876ULL},
			{r::STATE_AR4,0x1400},{r::STATE_AR5,0x1500},{r::STATE_AR6,0x1600}});
		auto final=observation("taken-only-PC-plus-four-frame-and-delay-effects",{{STATE_GENPC,base+0x606},{r::STATE_SP,0x3000},{r::STATE_RTN,taken ? 0x604U : 0x5555U},
			{r::STATE_ST0,u64(0xa608|(tc && !flag ? 0x1000 : 0))},{r::STATE_ST1,0x900},{r::STATE_A,b ? 0x123456789aULL : changed},{r::STATE_B,b ? changed : 0xfedcba9876ULL},
			{r::STATE_AR4,0x1401},{r::STATE_AR5,0x1500+unsigned(tc || !layout)},{r::STATE_AR6,0x1600+unsigned(taken)}});
		clocked_register_case c{{{0,std::move(initial)},{40,std::move(final)}},std::array<unsigned,5>{3,6,7,10,13}[(condition+flag+pattern+b+layout+unsigned(base!=0))%5],40};
		c.clear_pipeline_before_restore=true;c.data_events=std::vector<clocked_data_event>{};
		if(taken){c.data_events->push_back({true,0x2fff,0x604,7,true});c.data_events->push_back({false,0x2fff,0x604,0,false});c.volatile_data_reads={{0x2fff,0x604}};}
		c.program_events={{false,base+0x601,0x700,0,false},{false,base+0x602,delay0,0,false},{false,base+0x603,delay1,0,false}};
		if(taken)c.program_events.push_back({false,base+0x700,0x6d96,0,false});
		for(unsigned tick : {3U,4U,6U}){clocked_register_action change{};change.tick=tick;change.program={{base+(tick==3 ? 0x601U : tick==4 ? 0x602U : 0x603U),0xdead}};c.actions.push_back(change);}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
