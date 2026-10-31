// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_COUNT_RETIREMENTS
#include "c54x_section_common.h"
namespace c54x_test {
// Owners: BD, CALLD, RETD, BCD, RPTBD. All expectations are fixture-owned.
inline std::vector<clocked_register_case> delay_repeat_cases(unsigned owner)
{
	std::vector<clocked_register_case> cases;
	for(unsigned kind=0;kind<3;++kind)for(unsigned count : {0U,1U,8U,39U,255U})for(unsigned pattern=0;pattern<4;++pattern)
	for(bool staged : {false,true})for(unsigned condition=0;condition<(owner==3 ? 12U : 1U);++condition){
		unsigned const selector=kind+count+pattern+condition;
		unsigned const following=std::array<unsigned,3>{0,2,7}[selector%3];
		u32 const base=selector&1 ? 0x20000 : 0;
		u16 const target=selector&2 ? 0x500 : 0x700, body=std::array<u16,3>{0x8094,0xe5ab,0xf495}[kind], status=0x800|(pattern<<8);
		auto test=[=](u64 value){bool const negative=value&0x8000000000ULL,zero=!value;return std::array<bool,6>{!negative,negative,!zero,zero,!negative&&!zero,negative||zero}[condition%6];};
		u64 const tested=std::array<u64,4>{0,1,0xffffffffffULL,0x0080000000ULL}[pattern];
		bool const taken=owner!=3 || test(tested);
		u64 poison=0;for(u64 value : {0ULL,1ULL,0xffffffffffULL})if(test(value)!=taken){poison=value;break;}
		u16 const seed=std::array<u16,4>{0,0xffff,0x8001,0x0fff}[pattern];
		u64 const a=owner==3 && condition<6 ? tested : 0x1234560000ULL|seed, b=owner==3 && condition>=6 ? tested : 0xfeabcd0000ULL|u16(~seed);
		u64 const final_a=owner==3 && staged && condition<6 ? poison : a, final_b=owner==3 && staged && condition>=6 ? poison : b;
		u16 const body_pc=owner==2 ? 0x602 : 0x603, end=target==0x500 ? 0x604 : 0x605, continuation=taken ? target : 0x604;
		auto initial=initial_case("delay-owned-repeat-drains-before-fresh-continuation",{u16(owner==0 ? 0xf273 : owner==1 ? 0xf274 : owner==2 ? 0xfe00 : owner==3 ? 0xfa40|(condition<6 ? condition+2 : condition+4) : 0xf272),u16(owner==4 ? end : target)});
		for(auto &word : initial.program)word.address+=base;
		initial.program.insert(initial.program.end(),{{base+(owner==2 ? 0x601U : 0x602U),u16(0xec00|count)},{base+body_pc,body},{base+0x604,0xf4e1},
			{base+target,u16(0xec00|following)},{base+target+1,0x8193},{base+target+2,u16(owner==1 ? 0xfc00 : 0xf4e1)}});
		if(owner==2)initial.program.push_back({base+0x603,0xf495});
		if(owner==3)initial.program.insert(initial.program.end(),{{base+0x604,u16(0xec00|following)},{base+0x605,0x8193},{base+0x606,0xf4e1}});
		if(owner==4)initial.program.insert(initial.program.end(),{{base+0x604,0xf495},{base+end,0x8193},{base+end+1,0xf4e1}});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0xa608},{r::STATE_ST1,status},{r::STATE_A,a},{r::STATE_B,b},
			{r::STATE_SP,owner==2 ? 0x2fffU : 0x3000U},{r::STATE_T,0x5aa5},{r::STATE_BRC,owner==4 ? following : 13U},{r::STATE_RSA,0x5555},{r::STATE_REA,0x6666},{r::STATE_RC,0}});
		for(unsigned ar=0;ar<8;++ar)initial.initial.push_back({r::STATE_AR0+ar,ar==3 ? 0x2000U : ar==4 ? 0x1800U : ar==5 ? 0x1c00U : 0x4000+ar});
		if(owner==2)initial.data.push_back({0x2fff,target});
		auto final=observation("independent-delay-and-path-counts-with-balanced-stack",{{STATE_GENPC,base+(owner==4 ? end+2 : owner==1 ? 0x605 : continuation+3)},
			{r::STATE_ST0,0xa608},{r::STATE_ST1,status},{r::STATE_A,final_a},{r::STATE_B,final_b},{r::STATE_T,0x5aa5},{r::STATE_SP,0x3000},{r::STATE_RC,0},
			{r::STATE_BRC,owner==4 ? 0U : 13U},{r::STATE_RSA,owner==4 ? 0x604U : 0x5555U},{r::STATE_REA,owner==4 ? end : 0x6666U},
			{r::STATE_AR3,0x2001+following},{r::STATE_AR4,0x1800+(kind==2 ? 0 : count+1)},{r::STATE_AR5,0x1c00+(kind==1 ? count+1 : 0)}});
		for(unsigned i=0;i<256;++i){u16 const value=seed+i*257;initial.data.insert(initial.data.end(),{{0x1800+i,value},{0x1c00+i,0xdead}});
			final.expected_data.insert(final.expected_data.end(),{{0x1800+i,u16(kind==0 && i<=count ? final_a : value)},{0x1c00+i,u16(kind==1 && i<=count ? value : 0xdead)}});}
		for(unsigned i=0;i<32;++i){initial.data.push_back({0x2000+i,0xdead});final.expected_data.push_back({0x2000+i,u16(i<=following ? final_b : 0xdead)});}
		for(u16 at : {0x17ff,0x1900,0x1bff,0x1d00,0x1fff,0x2020}){initial.data.push_back({at,0x2468});final.expected_data.push_back({at,0x2468});}
		unsigned const finish=4*(count+following+2)+100,checkpoint=std::array<unsigned,4>{3,8,9+count/2,20+count}[selector%4];
		clocked_register_case c{{{0,std::move(initial)},{finish,std::move(final)}},checkpoint,finish};c.pipeline_frontend=staged;c.clear_pipeline_before_restore=true;
		c.retired_instruction=retired_instruction_check{base+body_pc,body,count+1};
		if(owner!=4){
			clocked_register_action change{};change.tick=owner==2 ? 8 : 5;
			if(owner==2)change.data={{0x2fff,0x620}};else change.program={{base+0x601,0x620}};c.actions.push_back(change);
		}
		if(staged && owner!=4){clocked_register_action change{};change.tick=6;change.program={{base+body_pc,0xe8ff}};
			if(owner==3)change.values={{r::STATE_A,final_a},{r::STATE_B,final_b}};
			c.actions.push_back(change);if(owner==2)std::swap(c.actions[0],c.actions[1]);
		}
		c.data_events=std::vector<clocked_data_event>{};
		if(owner==1)c.data_events->push_back({true,0x2fff,0x604,0,false});
		if(owner==2)c.data_events->push_back({false,0x2fff,target,0,false});
		for(unsigned i=0;i<=count;++i){
			if(kind==1){c.data_events->push_back({false,0x1800+i,u16(seed+i*257),0,false});if(staged && i)c.data_events->push_back({true,0x1c00+i-1,u16(seed+(i-1)*257),0,false});}
			if(kind==0)c.data_events->push_back({true,0x1800+i,u16(final_a),0,false});
			if(kind==1 && !staged)c.data_events->push_back({true,0x1c00+i,u16(seed+i*257),0,false});
		}
		if(kind==1 && staged)c.data_events->push_back({true,0x1c00+count,u16(seed+count*257),0,false});
		for(unsigned i=0;i<=following;++i)c.data_events->push_back({true,0x2000+i,u16(final_b),0,false});
		if(owner==1)c.data_events->push_back({false,0x2fff,0x604,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
