// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for(unsigned outer : {0U,1U,2U,7U}) for(unsigned inner : {1U,2U,4U,8U}) for(bool unconditional : {false,true})
	for(bool b : {false,true}) for(u32 page : {0U,0x20000U}) for(unsigned source=0;source<3;++source) {
		u16 const end=unconditional ? 0x60d : 0x60a, st1=source ? 0x100 : 0x900;
		unsigned const total=(outer+1)*inner;
		auto initial=initial_case("backward-inner-branch-retains-outer-repeat-controller",{0xf072,end});
		for(auto &word : initial.program)word.address+=page;
		initial.program.insert(initial.program.end(),{{page+0x602,u16(b ? 0xf120 : 0xf020)},{page+0x603,u16(inner-1)},
			{page+0x604,0xe589},{page+0x605,u16(b ? 0xf300 : 0xf000)},{page+0x606,0xffff},
			{page+0x607,u16((b ? 0xf84a : 0xf842)|unsigned(unconditional))},{page+0x608,u16(unconditional ? 0x60c : 0x604)},
			{page+end-1,0x6d94},{page+end+1,0xf4e1},
			{page+(source==2 ? 4U : 0x48U),0xf495},{page+(source==2 ? 5U : 0x49U),0xf4eb}});
		if(unconditional)initial.program.insert(initial.program.end(),{{page+0x609,0xf073},{page+0x60a,0x604}});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,page+0x600},{r::STATE_ST0,0xa002},{r::STATE_ST1,st1},
			{r::STATE_IMR,source==1 ? 4U : 0U},{r::STATE_SP,0x3000},{r::STATE_BRC,outer},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfeabcd9876ULL},{r::STATE_T,0x5555},
			{r::STATE_AR2,0x5000},{r::STATE_AR3,0x6000},{r::STATE_AR4,0x7000}});
		auto final=observation("inner-and-outer-counts-complete-independently",{{STATE_GENPC,page+end+2},{r::STATE_SP,0x3000},
			{r::STATE_BRC,0},{r::STATE_RSA,0x602},{r::STATE_REA,end},{r::STATE_ST0,0xa002},{r::STATE_ST1,st1},
			{r::STATE_AR2,0x5000+total},{r::STATE_AR3,0x6000+total},{r::STATE_AR4,0x7001+outer},
			{r::STATE_A,b ? 0x123456789aULL : 0xffffffffffULL},{r::STATE_B,b ? 0xffffffffffULL : 0xfeabcd9876ULL},{r::STATE_T,0x5555}});
		for(unsigned i=0;i<total;++i){initial.data.push_back({0x5000+i,u16(0x9000+i)});initial.data.push_back({0x6000+i,0xdead});final.expected_data.push_back({0x6000+i,u16(0x9000+i)});}
		clocked_register_case c{{{0,std::move(initial)},{2000,std::move(final)}},std::array<unsigned,4>{9,12,15,19}[(outer+inner+source+unsigned(b)+unsigned(unconditional)+(page>>16))%4],2000};
		c.initial_input_lines={{2,CLEAR_LINE},{INPUT_LINE_NMI,CLEAR_LINE}};c.data_events=std::vector<clocked_data_event>{};
		for(unsigned i=0;i<total;++i) {
			if(i==1 && source && unconditional && inner>1)continue;
			c.data_events->push_back({false,0x5000+i,u16(0x9000+i),0,false});c.data_events->push_back({true,0x6000+i,u16(0x9000+i),0,false});
			if(!i && source) {
				c.data_events->back().raise_input=source==2 ? INPUT_LINE_NMI : 2;
				if(unconditional && inner>1) {c.data_events->push_back({false,0x5001,0x9001,0,false});c.data_events->push_back({true,0x6001,0x9001,0,false});}
				clocked_data_event frame{true,0x2fff,0,0,false};frame.value_range=std::array<u16,2>{0x602,u16(end+1)};frame.value_register=r::STATE_RTN;
				c.data_events->push_back(frame);frame.write=false;c.data_events->push_back(frame);
				c.irqs={{source==2 ? INPUT_LINE_NMI : 2,0,~0U,unconditional && inner>1 ? 3U : 2U,{}}};
			}
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
