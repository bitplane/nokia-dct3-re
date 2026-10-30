// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_CONTEXT_POP_POISON
#include "c54x_section_common.h"
namespace c54x_test {
enum class context_repeat { body, leaf, stlm };
inline std::vector<clocked_register_case> context_repeat_cases(context_repeat kind)
{
	bool const leaf=kind!=context_repeat::body, stlm=kind==context_repeat::stlm;
	std::vector<clocked_register_case> cases;
	for (unsigned outer : {0U,1U,2U,7U,56U}) for (unsigned inner : {0U,1U,3U,63U}) for (bool after_end : {false,true})
	for (bool nmi : {false,true}) for (bool delayed : {false,true}) for (u32 page : {0U,0x20000U}) {
		if (!leaf && (!outer || after_end)) continue;
		u16 const end=after_end ? 0x609 : 0x60b;
		u32 const sp=0x3000+unsigned(leaf && ((outer+inner+unsigned(nmi)+unsigned(delayed)+(page>>16))&1)), frame_base=sp-(leaf ? 3 : 2);
		u16 const saved_count=outer-unsigned(after_end && outer), saved_status=0x900|(outer ? 0x8000 : 0);
		auto initial=initial_case("software-restored-block-repeat-real-ISR-return",{0xf272,end});
		initial.program.clear();
		for (unsigned at=0x600;at<0x640;++at) initial.program.push_back({page+at,0xf495});
		initial.program.insert(initial.program.end(),{{page+0x600,0xf272},{page+0x601,end},{page+0x604,0xe589},{page+0x60a,0x6d96},{page+end+1,0x6d97},{page+end+2,0xf4e1}});
		if (leaf) {
			initial.program.insert(initial.program.end(),{{page+0x605,0xe801},{page+0x606,0xfb44},{page+0x607,0x700}});
			for (unsigned at=0x700;at<0x720;++at) initial.program.push_back({page+at,0xf495});
			initial.program.insert(initial.program.end(),{{page+0x701,0x7695},{page+0x702,0x1357},{page+0x71c,0xfc00}});
		}
		u16 const vector=nmi ? 4 : 0x40;
		initial.program.insert(initial.program.end(),{{page+vector,0x4a1e},{page+vector+1,u16(0xf880|(page>>16))},{page+vector+2,0x900}});
		std::vector<u16> handler{0x4a06,0x4a07,0x4a1a,0x4a1b,0x4a1c,0xf495,0x771a,u16(inner),0xf072,0x90b,0x7694,0x55aa,0xf495,
			0x8a1c,0x8a1b,0x8a1a,0xf495,0xf495,0xf495,0xf495,0xf495,0xf495,0x8a07,0x8a06,u16(delayed ? 0xf6e5 : 0xf4e5),0xf495,0xf495};
		if (stlm) {
			handler.erase(handler.begin()+5);
			handler.insert(handler.begin()+5,{0x4807,0xf030,0x2000,0xf040,0x4900,0x8807,0xf495,0xf495});
			handler[16]=0x912; handler.insert(handler.begin()+29,{0xf020,1});
		}
		for (unsigned i=0;i<handler.size();++i) initial.program.push_back({page+0x900+i,handler[i]});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,page+0x600},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x100},{r::STATE_SP,sp},
			{r::STATE_IMR,1},{r::STATE_BRC,outer},{r::STATE_A,0},{r::STATE_B,0x2345}});
		for (unsigned ar=2;ar<8;++ar) initial.initial.push_back({r::STATE_AR0+ar,std::array<u16,6>{0x5000,0x6000,0x7100,u16(leaf ? 0x7000 : 0x1500),0x1600,0x1700}[ar-2]});
		auto final=observation("balanced-frame-and-restored-architectural-loop",{{STATE_GENPC,page+end+3},{r::STATE_SP,sp},{r::STATE_A,leaf ? 1U : 0U},{r::STATE_B,0x2345},
			{r::STATE_ST0,0xa608},{r::STATE_ST1,0x100},{r::STATE_BRC,0},{r::STATE_RSA,0x604},{r::STATE_REA,end},
			{r::STATE_AR2,0x5001+outer},{r::STATE_AR3,0x6001+outer},{r::STATE_AR4,0x7101+inner},{r::STATE_AR5,leaf ? 0x7001+outer : 0x1500U},
			{r::STATE_AR6,after_end ? 0x1600U : 0x1601+outer},{r::STATE_AR7,0x1701}});
		for (unsigned i=0;i<=outer;++i) { u16 const value=0x1357^(i*0x2345); initial.data.push_back({0x5000+i,value});initial.data.push_back({0x6000+i,0xdead}); final.expected_data.push_back({0x6000+i,value}); }
		for (unsigned i=0;i<=inner;++i) { initial.data.push_back({0x7100+i,0xdead}); final.expected_data.push_back({0x7100+i,0x55aa}); }
		if (leaf) for (unsigned i=0;i<=outer;++i) { initial.data.push_back({0x7000+i,0xdead}); final.expected_data.push_back({0x7000+i,0x1357}); }
		clocked_register_case c{{{0,std::move(initial)},{4096,std::move(final)}},4,4096};
		c.initial_input_lines={{0,CLEAR_LINE},{INPUT_LINE_NMI,CLEAR_LINE}};
		c.data_events=std::vector<clocked_data_event>{{false,0x5000,0x1357,0,false},{true,0x6000,0x1357,0,false}};
		if (leaf) { c.data_events->push_back({true,sp-1,0x60a,0,false}); c.data_events->push_back({true,0x7000,0x1357,0,false}); }
		c.data_events->back().raise_input=nmi ? INPUT_LINE_NMI : 0;
		clocked_data_event frame{true,sp-(leaf ? 2U : 1U),0,0,false}; frame.value_range=std::array<u16,2>{u16(leaf ? 0x703 : 0x605),u16(leaf ? 0x71b : 0x60b)};frame.value_register=r::STATE_RTN;c.data_events->push_back(frame);
		std::array<unsigned,5> boundaries{};
		std::array<u16,6> const regs{0x1e,6,7,0x1a,0x1b,0x1c}, values{u16(page>>16),0xa608,saved_status,saved_count,0x604,end};
		c.data_events->push_back({false,regs[0],values[0],0,false}); c.data_events->push_back({true,frame_base,values[0],0,false}); boundaries[0]=c.data_events->size();
		for (unsigned i=1;i<6;++i) {
			c.data_events->push_back({false,regs[i],values[i],0,false});
			if (i>1) c.data_events->push_back({true,frame_base-(i-1),values[i-1],0,false});
		}
		if (stlm) c.data_events->push_back({false,7,saved_status,0,false});
		c.data_events->push_back({true,frame_base-5,values[5],0,false});
		if (stlm) c.data_events->push_back({true,7,0x4900,0,false});
		c.data_events->push_back({true,0x1a,u16(inner),0,false});
		boundaries[1]=c.data_events->size()+1;
		for (unsigned i=0;i<=inner;++i) c.data_events->push_back({true,0x7100+i,0x55aa,0,false});
		for (unsigned saved=5;saved>=3;--saved) {
			c.data_events->push_back({false,frame_base-saved,values[saved],0,false});
			if (saved<5) c.data_events->push_back({true,regs[saved+1],values[saved+1],0,false});
		}
		c.data_events->push_back({true,regs[3],values[3],0,false});
		boundaries[2]=c.data_events->size();
		c.data_events->push_back({false,frame_base-2,values[2],0,false});
		c.data_events->push_back({false,frame_base-1,values[1],0,false});
		frame.write=false;
		if (delayed) c.data_events->push_back({false,frame_base,u16(page>>16),0,false});
		c.data_events->push_back({true,regs[2],values[2],0,false});
		boundaries[3]=c.data_events->size();
		c.data_events->push_back({true,regs[1],values[1],0,false});
		if (delayed) c.data_events->push_back(frame);
		if (!delayed) { c.data_events->push_back({false,frame_base,u16(page>>16),0,false}); c.data_events->push_back(frame); }
		boundaries[4]=c.data_events->size()+1;
		if (leaf) c.data_events->push_back({false,sp-1,0x60a,0,false});
		for (unsigned i=1;i<=outer;++i) {
			u16 const value=0x1357^(i*0x2345); c.data_events->push_back({false,0x5000+i,value,0,false});c.data_events->push_back({true,0x6000+i,value,0,false});
			if (leaf) { c.data_events->push_back({true,sp-1,0x60a,0,false}); c.data_events->push_back({true,0x7000+i,0x1357,0,false}); c.data_events->push_back({false,sp-1,0x60a,0,false}); }
		}
		c.irqs={{nmi ? INPUT_LINE_NMI : 0,0,~0U,leaf ? 4U : 2U,{}}};
		unsigned const cohort=cases.size()%6;
		if (cohort) c.checkpoint_after_data_events=boundaries[cohort-1];
		cases.push_back(std::move(c));
	}
	return cases;
}
}
