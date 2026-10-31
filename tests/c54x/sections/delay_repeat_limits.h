// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for(unsigned owner : {0U,3U,4U})for(unsigned kind=0;kind<8;++kind)for(u32 base : {0U,0x20000U}){
		if(owner && kind>0 && kind<4)continue;
		auto initial=initial_case("unqualified-delay-repeat-keeps-enclosing-control-effects",{u16(owner==4 ? 0xf272 : owner==3 ? 0xfa47 : 0xf273),u16(owner==4 ? 0x606 : 0x700),0xec00,0x8094,0xf4e1});
		for(auto &word : initial.program)word.address+=base;
		if(kind==0)initial.program.insert(initial.program.end(),{{base+0x602,0xf495},{base+0x603,0xec01}});
		if(kind==1)initial.program.push_back({base+0x600,0x6e87});
		if(kind==2)initial.program.insert(initial.program.end(),{{base+0x600,0xf274},{base+0x602,0xf495},{base+0x603,0xec01}});
		if(kind==3)initial.program.insert(initial.program.end(),{{base+0x600,0xfe00},{base+0x601,0xf495},{base+0x602,0xec01}});
		if(kind==4)initial.program.push_back({base+0x602,0xf070});
		if(kind==5)initial.program.push_back({base+0x602,0x4780});
		if(kind>=6)initial.program.insert(initial.program.end(),{{base+0x602,0xec01},{base+0x603,u16(kind==6 ? 0x76f8 : 0x71f8)}});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x800},{r::STATE_A,0x1234560000ULL},
			{r::STATE_B,0xfeabcdffffULL},{r::STATE_T,0x5aa5},{r::STATE_SP,0x3000},{r::STATE_RC,0},{r::STATE_BRC,13},{r::STATE_RSA,0x5555},{r::STATE_REA,0x6666}});
		for(unsigned ar=0;ar<8;++ar)initial.initial.push_back({r::STATE_AR0+ar,ar==3 ? 0x2000U : ar==4 ? 0x1800U : ar==5 ? 0x1c00U : 0x4000+ar});
		initial.data={{0x3000,0x700}};initial.expected.clear();
		auto final=observation("fail-closed-repeat-without-body-or-frame-write",{{r::STATE_ST0,0xa608},{r::STATE_ST1,owner==4 ? 0x8800U : 0x800U},{r::STATE_RC,0},{r::STATE_BRC,13},
			{r::STATE_SP,kind==2 ? 0x2fffU : kind==3 ? 0x3001U : 0x3000U},{r::STATE_T,0x5aa5},{r::STATE_A,0x1234560000ULL},{r::STATE_B,0xfeabcdffffULL}});
		final.expected.back().value=1;
		for(unsigned ar=0;ar<8;++ar)final.expected.push_back({int(r::STATE_AR0+ar),ar==3 ? 0x2000U : ar==4 ? 0x1800U : ar==5 ? 0x1c00U : 0x4000+ar});
		clocked_register_case c{{{0,std::move(initial)},{24,std::move(final)}},(kind+owner+unsigned(base!=0))&1 ? 3U : 10U,24};
		c.require_idle=false;c.clear_pipeline_before_restore=true;c.data_events=std::vector<clocked_data_event>{};
		if(kind==3)c.data_events->push_back({false,0x3000,0x700,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
