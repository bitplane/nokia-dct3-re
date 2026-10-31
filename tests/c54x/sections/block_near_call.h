// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_HIGH_PROGRAM
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for(unsigned count : {0U,1U,5U,15U}) for(unsigned layout=0;layout<3;++layout) for(u32 base : {0U,0x20000U,0x7f0000U})
	for(unsigned depth : {1U,2U}) for(unsigned source=0;source<3;++source) for(bool delayed_return : {false,true}) for(bool delayed : {false,true}) {
		unsigned const iterations=count+1;
		u16 const call=layout==0 ? 0x603 : layout==1 ? (delayed ? 0x606 : 0x608) : 0x602;
		u16 const end=layout==2 ? call+(delayed ? 3 : 1) : 0x609, continuation=call+(delayed ? 4 : 2), st1=source ? 0x100 : 0x900;
		auto initial=initial_case("block-near-call-endpoint-nesting-and-return-delays",{0xf072,end});
		for(auto &word : initial.program)word.address+=base;
		for(unsigned i=0;i<32;++i){initial.program.push_back({base+0x700+i,0xf495});initial.program.push_back({base+0x800+i,0xf495});}
		if(layout!=2)initial.program.push_back({base+0x602,0x6d94});
		initial.program.insert(initial.program.end(),{{base+call,u16(delayed ? 0xf274 : 0xf074)},{base+call+1,0x700},
			{base+end+1,0x6d91},{base+end+10,0xf4e1},{base+0x700,0xe589}});
		if(delayed || layout==0)initial.program.push_back({base+call+2,0x6d95});
		if(depth==2)initial.program.insert(initial.program.end(),{{base+0x701,0xf074},{base+0x702,0x800}});
		u32 const ret=base+(depth==2 ? 0x703 : 0x701);
		initial.program.push_back({ret,u16(delayed_return ? 0xfe00 : 0xfc00)});
		if(delayed_return)initial.program.push_back({ret+1,0x6d97});
		initial.program.insert(initial.program.end(),{{base+0x800,0x6d96},{base+0x801,u16(delayed_return ? 0xfe00 : 0xfc00)},
			{base+(source==2 ? 4U : 0x48U),0xf495},{base+(source==2 ? 5U : 0x49U),0xf4eb}});
		if(delayed_return)initial.program.push_back({base+0x802,0x6d97});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0xa608},{r::STATE_ST1,st1},{r::STATE_IMR,source==1 ? 4U : 0U},
			{r::STATE_SP,0x3000},{r::STATE_BRC,count},{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfeabcd9876ULL},{r::STATE_T,0x5555}});
		for(unsigned ar=0;ar<8;++ar)initial.initial.push_back({r::STATE_AR0+ar,ar==2 ? 0x5000U : ar==3 ? 0x6000U : 0x6c00+0x100*ar});
		auto final=observation("balanced-near-frames-and-exact-caller-fallthrough",{{STATE_GENPC,base+end+11},{r::STATE_SP,0x3000},{r::STATE_BRC,0},{r::STATE_RSA,0x602},{r::STATE_REA,end},
			{r::STATE_ST0,0xa608},{r::STATE_ST1,st1},{r::STATE_AR1,0x6d01},{r::STATE_AR2,0x5000+iterations},{r::STATE_AR3,0x6000+iterations},
			{r::STATE_AR4,0x7000+(layout!=2 ? iterations : 0)},{r::STATE_AR5,0x7100+(delayed || layout==0 ? iterations : 0)},
			{r::STATE_AR6,0x7200+(depth==2 ? iterations : 0)},{r::STATE_AR7,0x7300+(delayed_return ? depth*iterations : 0)},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfeabcd9876ULL},{r::STATE_T,0x5555}});
		for(unsigned i=0;i<iterations;++i){initial.data.push_back({0x5000+i,u16(0x9000+i)});initial.data.push_back({0x6000+i,0xdead});final.expected_data.push_back({0x6000+i,u16(0x9000+i)});}
		clocked_register_case c{{{0,std::move(initial)},{1800,std::move(final)}},std::array<unsigned,4>{5,8,13,25}[(count+layout+depth+source+delayed+delayed_return)%4],1800};
		c.initial_input_lines={{2,CLEAR_LINE},{INPUT_LINE_NMI,CLEAR_LINE}};c.data_events=std::vector<clocked_data_event>{};
		for(unsigned i=0;i<iterations;++i){
			c.data_events->push_back({true,0x2fff,continuation,0,false});
			c.data_events->push_back({false,0x5000+i,u16(0x9000+i),0,false});
			if(!i && source)c.data_events->back().raise_input=source==2 ? INPUT_LINE_NMI : 2;
			bool const deferred=depth==1 && layout==2 && delayed_return && iterations>1;
			// Whole-block RETD defers entry: CALL permits the next callee Read;
			// CALLD drains the block. IACK can precede the pending copy Write.
			bool const entry=source && deferred && i==(delayed ? iterations-1 : 1);
			if(entry){
				if(delayed)c.data_events->push_back({false,0x2fff,continuation,0,false});
				c.data_events->push_back({true,0x6000+i,u16(0x9000+i),0,false});
				unsigned const before=c.data_events->size();
				clocked_data_event frame{true,delayed ? 0x2fffU : 0x2ffeU,0,0,false};frame.value_range=std::array<u16,2>{0x602,0x804};frame.value_register=r::STATE_RTN;
				c.data_events->push_back(frame);frame.write=false;c.data_events->push_back(frame);
				c.irqs={{source==2 ? INPUT_LINE_NMI : 2,0,~0U,before-unsigned(!delayed),{}}};
			}
			// A Read-owned RET pop precedes the preceding copy's Execute Write.
			if(depth==1 && !(entry && delayed))c.data_events->push_back({false,0x2fff,continuation,0,false});
			if(!entry)c.data_events->push_back({true,0x6000+i,u16(0x9000+i),0,false});
			if(depth==2)c.data_events->push_back({true,0x2ffe,0x703,0,false});
			if(!i && source && !deferred){
				unsigned const before=c.data_events->size();
				clocked_data_event frame{true,depth==2 ? 0x2ffdU : 0x2fffU,0,0,false};frame.value_range=std::array<u16,2>{0x602,0x804};frame.value_register=r::STATE_RTN;
				c.data_events->push_back(frame);frame.write=false;c.data_events->push_back(frame);
				c.irqs={{source==2 ? INPUT_LINE_NMI : 2,0,~0U,before,{}}};
			}
			if(depth==2){c.data_events->push_back({false,0x2ffe,0x703,0,false});c.data_events->push_back({false,0x2fff,continuation,0,false});}
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
