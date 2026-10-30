// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> block_far_call_cases(bool delayed)
{
	std::vector<clocked_register_case> cases;
	for(unsigned count : {0U,1U,2U,7U}) for(bool late : {false,true}) for(u32 caller : {0U,0x20000U}) for(bool cross : {false,true})
	for(unsigned depth : {1U,2U}) for(unsigned delay=0;delay<(delayed ? 3U : 1U);++delay) for(unsigned source=0;source<3;++source) {
		u32 const callee=(cross ? caller^0x20000U : caller)|0x700, nested=((callee^0x20000U)&0x7f0000)|0x800;
		u16 const call=late ? 0x606 : 0x603, continuation=call+(delayed ? 4 : 2), st1=source ? 0x100 : 0x900;
		unsigned const iterations=count+1;
		auto initial=initial_case("block-repeat-far-call-nested-frame-and-interrupt-drain",{0xf072,0x609});
		for(auto &word : initial.program)word.address+=caller;
		initial.program.insert(initial.program.end(),{{caller+0x602,0x6d94},{caller+call,u16((delayed ? 0xfb80 : 0xf980)|(callee>>16))},{caller+call+1,0x700},
			{caller+call+2,u16(!delayed || !delay ? 0x6d95 : delay==1 ? 0x770e : 0xe801)},
			{caller+0x60a,0xf4e1},{callee,0xe589},{callee+1,u16(depth==2 ? 0xf980|(nested>>16) : 0xf4e4)},
			{callee+2,0x800},{callee+3,0xf4e4},{nested,0x6d96},{nested+1,0xf4e4}});
		if(delayed)initial.program.push_back({caller+call+3,u16(!delay ? 0x6d97 : delay==1 ? 0x1234 : 0xf495)});
		for(u32 page : {0U,0x20000U}) {initial.program.push_back({page+(source==2 ? 4U : 0x48U),0xf495});initial.program.push_back({page+(source==2 ? 5U : 0x49U),0xf4eb});}
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,caller+0x600},{r::STATE_ST0,0xa608},{r::STATE_ST1,st1},{r::STATE_IMR,source==1 ? 4U : 0U},
			{r::STATE_SP,0x3000},{r::STATE_BRC,count},{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfeabcd9876ULL},{r::STATE_T,0x5555}});
		for(unsigned ar=0;ar<8;++ar)initial.initial.push_back({r::STATE_AR0+ar,ar==2 ? 0x5000U : ar==3 ? 0x6000U : ar==4 ? 0x7000U : ar==5 ? 0x7100U : 0x7200+ar});
		auto final=observation("caller-controller-delay-effects-and-all-frames-settle",{{STATE_GENPC,caller+0x60b},{r::STATE_SP,0x3000},{r::STATE_BRC,0},{r::STATE_RSA,0x602},{r::STATE_REA,0x609},
			{r::STATE_ST0,0xa608},{r::STATE_ST1,st1},{r::STATE_AR2,0x5000+iterations},{r::STATE_AR3,0x6000+iterations},{r::STATE_AR4,0x7000+iterations},
			{r::STATE_AR5,0x7100+(!delayed || !delay ? iterations : 0)},{r::STATE_AR6,0x7206+(depth==2 ? iterations : 0)},{r::STATE_AR7,0x7207+(delayed && !delay ? iterations : 0)},
			{r::STATE_A,delayed && delay==2 ? 1ULL : 0x123456789aULL},{r::STATE_B,0xfeabcd9876ULL},{r::STATE_T,delayed && delay==1 ? 0x1234U : 0x5555U}});
		for(unsigned i=0;i<iterations;++i){initial.data.push_back({0x5000+i,u16(0x9000+i)});initial.data.push_back({0x6000+i,0xdead});final.expected_data.push_back({0x6000+i,u16(0x9000+i)});}
		clocked_register_case c{{{0,std::move(initial)},{1000,std::move(final)}},std::array<unsigned,4>{8,10,15,23}[(count+unsigned(late)+unsigned(cross)+depth+delay+source)%4],1000};
		c.initial_input_lines={{2,CLEAR_LINE},{INPUT_LINE_NMI,CLEAR_LINE}};c.data_events=std::vector<clocked_data_event>{};
		for(unsigned i=0;i<iterations;++i) {
			c.data_events->push_back({true,0x2fff,continuation,0,false});
			// Read-owned STM publication precedes the call's Execute-owned page word.
			if(delayed && delay==1)c.data_events->push_back({true,0xe,0x1234,0,false});
			c.data_events->push_back({true,0x2ffe,u16(caller>>16),0,false});
			c.data_events->push_back({false,0x5000+i,u16(0x9000+i),0,false});
			if(!i && source)c.data_events->back().raise_input=source==2 ? INPUT_LINE_NMI : 2;
			c.data_events->push_back({true,0x6000+i,u16(0x9000+i),0,false});
			if(depth==2){c.data_events->push_back({true,0x2ffd,0x703,0,false});c.data_events->push_back({true,0x2ffc,u16(callee>>16),0,false});}
			else{c.data_events->push_back({false,0x2ffe,u16(caller>>16),0,false});c.data_events->push_back({false,0x2fff,continuation,0,false});}
			if(!i && source) {
				// The first FRET can finish before entry; nested FCALL instead retains both frames.
				unsigned const before=c.data_events->size();
				clocked_data_event frame{true,depth==2 ? 0x2ffbU : 0x2fffU,0,0,false};frame.value_range=std::array<u16,2>{0x602,0x802};frame.value_register=r::STATE_RTN;
				c.data_events->push_back(frame);frame.write=false;c.data_events->push_back(frame);
				c.irqs={{source==2 ? INPUT_LINE_NMI : 2,0,~0U,before,{}}};
			}
			if(depth==2){c.data_events->push_back({false,0x2ffc,u16(callee>>16),0,false});c.data_events->push_back({false,0x2ffd,0x703,0,false});c.data_events->push_back({false,0x2ffe,u16(caller>>16),0,false});c.data_events->push_back({false,0x2fff,continuation,0,false});}
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
