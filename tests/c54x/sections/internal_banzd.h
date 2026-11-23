// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
// Short BANZD inside RPTB: the target is in the block, both delay words run
// on either outcome, and only Decode's endpoint owns BRC.
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for(unsigned count : {0U,1U,2U,7U})for(unsigned inner : {0U,1U,3U})for(bool forward : {false,true})for(u32 base : {0U,0x20000U}){
		auto initial=initial_case(forward ? "internal-banzd-forward" : "internal-banzd-backward",{0xf072,0x60b});
		for(auto &entry : initial.program)entry.address+=base;
		initial.program.insert(initial.program.end(),{{base+0x602,0x7715},{base+0x603,u16(inner)},{base+0x604,0x6d94},
			{base+0x605,0x6e8d},{base+0x606,u16(forward ? 0x60b : 0x604)},{base+0x607,0x6d92},{base+0x608,0x6d93},
			{base+0x609,0x6d96},{base+0x60a,0xf495},{base+0x60b,0x6d97},{base+0x60c,0xf4e1}});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x900},{r::STATE_BRC,count}});
		for(unsigned ar=2;ar<8;++ar)initial.initial.push_back({r::STATE_AR0+ar,0x1000+ar*0x100});
		unsigned const loops=count+1, passes=forward ? loops : loops*(inner+1);
		auto final=observation("delay-words-run-per-branch-and-count-per-endpoint",{{STATE_GENPC,base+0x60d},{r::STATE_BRC,0},{r::STATE_RSA,0x602},{r::STATE_REA,0x60b},
			{r::STATE_ST1,0x900},{r::STATE_AR2,0x1200+passes},{r::STATE_AR3,0x1300+passes},{r::STATE_AR4,0x1400+passes},
			{r::STATE_AR5,u16(forward ? inner-1 : 0xffff)},{r::STATE_AR6,0x1600+(forward && inner ? 0 : loops)},{r::STATE_AR7,0x1700+loops}});
		cases.push_back({{{0,std::move(initial)},{512,std::move(final)}},std::array<unsigned,4>{7,10,12,16}[(count+inner+forward+unsigned(base!=0))%4],512});
	}
	return cases;
}
}
