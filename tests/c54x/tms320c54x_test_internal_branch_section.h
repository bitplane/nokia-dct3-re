// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
// Modes: signed BCD, BD, TC/NTC BCD, extended BANZ.
inline std::vector<clocked_register_case> internal_branch_cases(unsigned mode)
{
	std::vector<clocked_register_case> cases;
	for(unsigned count : {0U,1U,2U,7U})for(unsigned pattern=0;pattern<(mode==3 ? 8U : 4U);++pattern)
	for(unsigned condition=0;condition<(mode==0 ? 12U : mode==3 ? 3U : 2U);++condition)
	for(unsigned wide=0;wide<(mode==3 ? 1U : 2U);++wide)for(u32 base : {0U,0x20000U}){
		bool const b=mode==0 ? condition>=6 : mode==3 ? false : bool(condition);
		unsigned const source_pattern=mode==3 ? pattern/4+2*unsigned(base!=0) : pattern;
		auto word=[=](unsigned i)->u16{return source_pattern==0 ? 0 : source_pattern==1 ? 1 : source_pattern==2 ? 0xffff : i&1 ? 0 : 0x8000;};
		int const step=std::array<s16,8>{0,-1,1,1,0,s16(0xf495),17,s16(0x8000)}[pattern];
		u16 const ar0=mode==3 ? (condition==2 ? std::array<u16,8>{0,1,0x7fff,0,1,0x234,7,0}[pattern] : std::array<u16,8>{0,1,0xffff,0,1,0x1234,7,0x8000}[pattern]) : 0;
		u16 current=ar0;std::vector<bool> taken;
		for(unsigned i=0;i<=count;++i){
			bool decision;
			if(mode==3){int sum=int(current)+step;u16 const resolved=condition==2 ? u16((sum%32768+32768)%32768) : u16(sum);decision=resolved!=0;if(condition)current=resolved;}
			else if(mode==2)decision=bool(word(i)&0x8000)==b;
			else if(mode==1)decision=true;
			else {int const value=s16(word(i));decision=std::array<bool,6>{value>=0,value<0,value!=0,value==0,value>0,value<=0}[condition%6];}
			taken.push_back(decision);
		}
		auto initial=initial_case("internal-delayed-branch-retains-selected-arm",{0xf272,0x610});
		for(auto &entry : initial.program)entry.address+=base;
		initial.program.insert(initial.program.end(),{{base+0x604,u16(mode==2 ? 0x3492 : b ? 0x1192 : 0x1092)},
			{base+0x605,u16(mode==3 ? 0x6ce0+condition*8 : mode==2 ? (b ? 0xfa30 : 0xfa20) : mode==1 ? 0xf273 : 0xfa42+condition%6+(b ? 8 : 0))},
			{base+0x606,u16(mode==3 ? step : 0x60d)},{base+0x607,u16(mode==3 ? 0x60d : wide ? (b ? 0xf300 : 0xf000) : mode==2 ? (b ? 0xf4bc : 0xf5bc) : 0x6d94)},
			{base+0x608,u16(wide ? 1 : 0x6d95)},{base+0x609,0x7693},{base+0x60a,0x1111},{base+0x60b,0xf073},{base+0x60c,0x60f},
			{base+0x60d,0x7693},{base+0x60e,0x2222},{base+0x60f,0x6d96},{base+0x611,0x6d97},{base+0x612,0xf4e1}});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x900},{r::STATE_BRC,count},{r::STATE_A,0x1234},{r::STATE_B,0x2345},{r::STATE_T,0}});
		for(unsigned ar=2;ar<8;++ar)initial.initial.push_back({r::STATE_AR0+ar,ar==2 ? 0x5000U : ar==3 ? 0x6000U : 0x1000+ar*0x100});
		if(mode==3)initial.initial.insert(initial.initial.end(),{{r::STATE_AR0,ar0},{r::STATE_BK,condition==2 ? 0x8000U : 16U}});
		unsigned falls=0;for(bool decision : taken)falls+=!decision;
		u64 const result=mode==2 ? (b ? 0x2345 : 0x1234)+wide*(count+1) : u64(s64(s16(word(count)))+wide)&0xffffffffffULL;
		u16 const st0=mode==2 ? 0xa608|((wide ? bool(word(count)&0x8000) : !b) ? 0x1000 : 0) : (0xa608&~0x800)|(wide && word(count)==0xffff ? 0x800 : 0);
		auto final=observation("one-arm-per-loop-and-owned-delay-address-effects",{{STATE_GENPC,base+0x613},{r::STATE_BRC,0},{r::STATE_RSA,0x604},{r::STATE_REA,0x610},{r::STATE_ST0,st0},{r::STATE_ST1,0x900},
			{int(b ? r::STATE_B : r::STATE_A),result},{int(b ? r::STATE_A : r::STATE_B),b ? 0x1234U : 0x2345U},{r::STATE_AR2,0x5001+count},{r::STATE_AR3,0x6001+count},
			{r::STATE_AR4,0x1400+(wide || mode==2 || mode==3 ? 0 : count+1)},{r::STATE_AR5,0x1500+(mode==3 ? falls : wide ? 0 : count+1)},{r::STATE_AR6,0x1601+count},{r::STATE_AR7,0x1701}});
		if(mode==3)final.expected.push_back({r::STATE_AR0,current});
		for(unsigned i=0;i<=count+1;++i){initial.data.insert(initial.data.end(),{{0x5000+i,word(i)},{0x6000+i,0xdead}});final.expected_data.push_back({0x6000+i,u16(i<=count ? (taken[i] ? 0x2222 : 0x1111) : 0xdead)});}
		clocked_register_case c{{{0,std::move(initial)},{512,std::move(final)}},std::array<unsigned,4>{7,10,12,16}[(count+pattern+condition+wide+unsigned(base!=0))%4],512};
		c.data_events=std::vector<clocked_data_event>{};
		for(unsigned i=0;i<=count;++i){
			clocked_data_event read{false,0x5000+i,word(i),0,false};read.registers={{r::STATE_BRC,count-i},{r::STATE_ST1,u64(0x900|(i<count ? 0x8000 : 0))}};c.data_events->push_back(read);
			c.volatile_data_reads.push_back({0x5000+i,word(i)});c.data_events->push_back({true,0x6000+i,u16(taken[i] ? 0x2222 : 0x1111),0,false});
			c.program_events.push_back({false,base+(mode==3 ? 0x607U : 0x606U),0x60d,0,false});
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
