// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for(unsigned count : {0U,1U,2U,7U}) for(unsigned pattern=0;pattern<4;++pattern) for(bool endpoint : {false,true})
	for(bool b : {false,true}) for(u32 base : {0U,0x20000U}) for(unsigned source=0;source<3;++source){
		auto value=[=](unsigned i)->u16{return pattern==0 ? 0 : pattern==1 ? 1 : pattern==2 ? i&1 : i==count ? 0 : 1;};
		u16 const end=endpoint ? 0x607 : 0x60d, tail=end+1, st1=source ? 0x100 : 0x900;
		auto initial=initial_case("internal-block-branch-selected-arms-and-interrupt-return",{0xf072,end});
		for(auto &word : initial.program)word.address+=base;
		initial.program.push_back({base+0x602,u16(b ? 0x1192 : 0x1092)});
		if(endpoint)initial.program.insert(initial.program.end(),{{base+0x603,0x7693},{base+0x604,0x1111},{base+0x605,u16(b ? 0xf84d : 0xf845)},{base+0x606,0x607}});
		else initial.program.insert(initial.program.end(),{{base+0x603,u16(b ? 0xf84d : 0xf845)},{base+0x604,0x60a},
			{base+0x605,0x7693},{base+0x606,0x1111},{base+0x607,0xf073},{base+0x608,0x60c},
			{base+0x60a,0x7693},{base+0x60b,0x2222},{base+0x60c,u16(b ? 0xf300 : 0xf000)},{base+0x60d,1}});
		// A subsequent ordinary branch overwrites the live held-branch tags.
		initial.program.insert(initial.program.end(),{{base+tail,u16(b ? 0xf84a : 0xf842)},{base+tail+1,u16(tail+2)},{base+tail+2,0xf4e1},
			{base+(source==2 ? 4U : 0x48U),0xf495},{base+(source==2 ? 5U : 0x49U),0xf4eb}});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0xa608},{r::STATE_ST1,st1},{r::STATE_IMR,source==1 ? 4U : 0U},
			{r::STATE_A,0x1234},{r::STATE_B,0x2345},{r::STATE_SP,0x3000},{r::STATE_AR2,0x5000},{r::STATE_AR3,0x5100},{r::STATE_BRC,count}});
		auto final=observation("both-branch-arms-join-one-endpoint",{{STATE_GENPC,base+tail+3},{r::STATE_ST0,0xa608},{r::STATE_ST1,st1},{r::STATE_SP,0x3000},
			{r::STATE_AR2,0x5001+count},{r::STATE_AR3,0x5101+count},{r::STATE_BRC,0},{r::STATE_RSA,0x602},{r::STATE_REA,end},
			{int(b ? r::STATE_B : r::STATE_A),value(count)+unsigned(!endpoint)},{int(b ? r::STATE_A : r::STATE_B),b ? 0x1234U : 0x2345U}});
		for(unsigned i=0;i<=count;++i){initial.data.insert(initial.data.end(),{{0x5000+i,value(i)},{0x5100+i,0xaaaa}});final.expected_data.push_back({0x5100+i,u16(!endpoint && !value(i) ? 0x2222 : 0x1111)});}
		clocked_register_case c{{{0,std::move(initial)},{256,std::move(final)}},std::array<unsigned,8>{3,8,9,10,11,12,15,24}[(count+pattern+endpoint+b+source+unsigned(base!=0))%8],256};
		c.initial_input_lines={{2,CLEAR_LINE},{INPUT_LINE_NMI,CLEAR_LINE}};c.data_events=std::vector<clocked_data_event>{};
		for(unsigned i=0;i<=count;++i){
			clocked_data_event read{false,0x5000+i,value(i),0,false};read.registers={{r::STATE_BRC,count-i},{r::STATE_ST1,u64(i<count ? 0x8000 : 0),0x8000}};
			c.data_events->push_back(read);
			// The selected arm can expose the next loop Read before IACK;
			// an already issued store still drains before the hardware frame.
			bool const entry=source && ((count && (endpoint ? bool(value(0)) : !value(0))) ? i==1 : i==0);
			bool const drain=entry && i && !endpoint && value(i);
			if(entry && i){
				if(drain)c.data_events->push_back({true,0x5100+i,0x1111,0,false});
				unsigned const before=c.data_events->size();clocked_data_event frame{true,0x2fff,0,0,false};frame.value_range=std::array<u16,2>{0x602,0x610};frame.value_register=r::STATE_RTN;
				c.data_events->push_back(frame);frame.write=false;c.data_events->push_back(frame);c.irqs={{source==2 ? INPUT_LINE_NMI : 2,0,~0U,before-unsigned(drain),{}}};
			}
			if(!drain)c.data_events->push_back({true,0x5100+i,u16(!endpoint && !value(i) ? 0x2222 : 0x1111),0,false});
			if(!i && source)c.data_events->back().raise_input=source==2 ? INPUT_LINE_NMI : 2;
			if(entry && !i){
				unsigned const before=c.data_events->size();clocked_data_event frame{true,0x2fff,0,0,false};frame.value_range=std::array<u16,2>{0x602,0x610};frame.value_register=r::STATE_RTN;
				c.data_events->push_back(frame);frame.write=false;c.data_events->push_back(frame);c.irqs={{source==2 ? INPUT_LINE_NMI : 2,0,~0U,before,{}}};
			}
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
