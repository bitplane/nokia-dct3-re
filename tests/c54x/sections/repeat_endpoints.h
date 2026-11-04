// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_INSTRUCTION_EVENTS
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index=0;index<12;++index) {
		u16 const body=std::array<u16,3>{0x802,0xfffd,0xfffe}[index%3];
		bool const long_count=(index/3)&1, nmi=index>=6;
		u16 const start=body-(long_count ? 2 : 1), continuation=body+2;
		auto initial=initial_case("repeat-final-copy-protected-at-page-endpoints",{0xf4e1});
		initial.program={{0x10000U|start,u16(long_count ? 0xf070 : 0xec02)},
			{0x10000U|body,0x7312},{0x10000U|u16(body+1),0x600},{0x10000U|continuation,0xf4e1},
			{nmi ? 0x10004U : 0x10040U,0xf4e1}};
		if (long_count) initial.program.push_back({0x10000U|u16(start+1),2});
		initial.opcode=0x7312;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,0x10000U|start},{r::STATE_AR2,0x1234},{r::STATE_ST1,0},{r::STATE_IMR,1}});
		initial.data={{0x600,0xdead},{0x601,0xdead},{0x602,0xdead},{0x603,0xdead},{0xfff,0xdead}};
		auto final=observation("three-copies-before-one-IRQ-frame",{{STATE_GENPC,nmi ? 0x10005U : 0x10041U},
			{r::STATE_SP,0xfff},{r::STATE_AR2,0x1234},{r::STATE_RC,0}},
			{{0x600,0x1234},{0x601,0x1234},{0x602,0x1234},{0x603,0xdead},{0xfff,continuation}});
		clocked_register_case c{{{0,std::move(initial)},{128,std::move(final)}},1,128};
		c.pipeline_frontend=true;
		c.initial_input_lines={{0,CLEAR_LINE},{INPUT_LINE_NMI,CLEAR_LINE}};
		if (index&1) c.checkpoint_after_data_events=7;
		if(long_count)c.program_events.push_back({false,0x10000U|u16(start+1),2,0,false});
		c.program_events.push_back({false,0x10000U|body,0x7312,0,false});
		c.program_events.push_back({false,0x10000U|u16(body+1),0x600,0,false});
		c.instruction_events=std::vector<memory_word>{{0x10000U|start,u16(long_count ? 0xf070 : 0xec02)}};
		for(unsigned i=0;i<3;++i)c.instruction_events->push_back({0x10000U|body,0x7312});
		c.instruction_events->push_back({nmi ? 0x10004U : 0x10040U,0xf4e1});
		c.data_events=std::vector<clocked_data_event>{};
		for(unsigned i=0;i<3;++i) {
			c.data_events->push_back({false,0x12,0x1234,0,false});
			if(i)c.data_events->push_back({true,0x5ff+i,0x1234,0,false});
		}
		c.data_events->back().raise_input=nmi ? INPUT_LINE_NMI : 0;
		c.data_events->push_back({true,0x602,0x1234,0,false});
		c.data_events->push_back({true,0xfff,continuation,0,false});
		c.irqs={{nmi ? INPUT_LINE_NMI : 0,0,~0U,6,continuation}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
