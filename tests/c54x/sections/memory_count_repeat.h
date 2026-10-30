// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_COUNT_RETIREMENTS
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	auto append=[&](unsigned form,unsigned count,bool compiler,bool compatibility,unsigned interrupt,u32 base,unsigned checkpoint) {
		bool const absolute=form>=34;
		u16 const mode=form<2 ? (form ? 0x7f : 0) : form<26 ? 0x80+form-2 : form<34 ? 0x10+form-26 : 0xf8;
		unsigned const encoded=mode&7, selected=compatibility && !encoded ? 5 : encoded;
		unsigned const modification=form>=2 && form<26 ? (mode&0x78)>>3 : 0;
		u16 const st0=0xa600|(form>=26 && form<34 ? 0 : 8), st1=(compiler ? 0x4000 : 0)|(compatibility ? 0x20 : 0)|(interrupt ? 0 : 0x800);
		u16 const address=absolute ? (form==35 ? 0x15 : 0x5000) : form>=2 && form<26 ? 0x5000 :
			compiler ? 0x3000+mode : ((st0&0x1ff)<<7)|mode;
		u32 const body=base+(absolute ? 0x602 : 0x601);
		u16 const final_st0=compatibility && form>=2 && form<26 && encoded ? (st0&~0xe000)|(encoded<<13) : st0;
		auto initial=initial_case("memory-count-RPT-fetches-and-retires-one-retained-body",{u16(0x4700|mode)});
		if (absolute) initial.program[1].value=address;
		initial.program[15].value=0xf4e1; initial.opcode=0xf495;
		for (auto &word : initial.program) word.address+=base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_SP,0x3000},{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfedcba9876ULL},{r::STATE_RC,0},{r::STATE_BRC,13},
			{r::STATE_IMR,interrupt==1 ? 4U : 0U}});
		auto final=observation("sampled-count-pointer-and-interrupt-state-settle",{{STATE_GENPC,base+0x610},{r::STATE_RC,0},{r::STATE_BRC,13},
			{r::STATE_SP,0x3000},{r::STATE_ST0,final_st0},{r::STATE_ST1,st1},{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfedcba9876ULL}});
		for (unsigned i=0;i<8;++i) {
			u16 const seed=address==0x10+i ? count : 0x5000;
			initial.initial.push_back({r::STATE_AR0+i,seed});
			final.expected.push_back({int(r::STATE_AR0+i),address==0x10+i ? count :
				u16(0x5000+(form>=2 && form<26 && i==selected ? modification==1 ? -1 : modification==2 ? 1 : 0 : 0))});
		}
		if (address>=0x60) initial.data={{address,u16(count)}};
		int const line=interrupt==2 ? INPUT_LINE_NMI : 2;
		if (interrupt) {
			u32 const vector=base+(interrupt==2 ? 4 : 0x48);
			initial.program.insert(initial.program.end(),{{vector,0xf495},{vector+1,0xf4eb}});
		}
		clocked_register_case c{{{0,std::move(initial)},{1,observation("memory-count-repeat-running",{})},{count+80,std::move(final)}},checkpoint,count+80};
		c.single_fetch=body; c.retired_instruction=retired_instruction_check{body,0xf495,count+1};
		c.initial_input_lines={{2,CLEAR_LINE},{INPUT_LINE_NMI,CLEAR_LINE}};
		if (interrupt) c.actions={{2,{},false,{},{{line,ASSERT_LINE},{line,CLEAR_LINE}}}};
		c.actions.push_back({absolute ? 5U : 4U,{}}); c.actions.back().program={{body,0xe8ff}};
		unsigned const read=absolute ? 6 : 5;
		if (address>=0x60) { c.actions.push_back({read,{}}); c.actions.back().data={{address,0xdead}}; }
		c.data_events=std::vector<clocked_data_event>{{false,address,u16(count),read}};
		if (interrupt) {
			c.irqs={{line,0,~0U,1}};
			for (bool write : {true,false}) {
				clocked_data_event frame{write,0x2fff,0,0,false};
				frame.value_range=std::array<u16,2>{u16(body+1),0x60f}; frame.value_register=r::STATE_RTN;
				c.data_events->push_back(std::move(frame));
			}
		}
		cases.push_back(std::move(c));
	};
	for (unsigned form=0;form<36;++form) for (unsigned count : {0,1,7,255})
	for (unsigned compiler=0;compiler<2;++compiler) for (unsigned compatibility=0;compatibility<2;++compatibility)
	for (unsigned interrupt=0;interrupt<3;++interrupt) {
		u32 const base=cases.size()&1 ? 0x20000 : 0;
		append(form,count,compiler,compatibility,interrupt,base,std::array<unsigned,3>{3,6,12}[cases.size()%3]);
	}
	append(0,0x8000,false,false,0,0,3);
	append(0,0xffff,false,false,0,0x20000,6);
	return cases;
}
}
