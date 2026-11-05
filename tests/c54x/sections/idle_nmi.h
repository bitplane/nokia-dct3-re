// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (u16 idle : {0xf4e1,0xf6e1,0xf5e1}) for (unsigned mask=0;mask<2;++mask) for (unsigned intm=0;intm<2;++intm) {
		u16 const imr=mask ? 0xffff : 0, st1=intm ? 0x800 : 0;
		auto initial=initial_case("NMI-wakes-IDLE-independent-of-mask-and-INTM",{idle,0xf4e1});
		for (auto &word : initial.program) word.address+=0x10380;
		// Check pending flags at entry, then acknowledge them before terminal IDLE.
		initial.program.insert(initial.program.end(),{{0x10004,0xf020},{0x10005,0x8008},{0x10006,0x8801},{0x10007,0xf4e1}});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,0x10980},{r::STATE_ST1,st1},{r::STATE_IMR,imr},{r::STATE_SP,0x1000}});
		initial.data={{0xfff,0x5a5a}};
		auto asleep=observation("IDLE-remains-asleep-before-NMI",{{r::STATE_IDLE,1},{STATE_GENPC,0x10981},{r::STATE_SP,0x1000}});
		auto pending=observation("NMI-latched-but-vector-entry-held",{{r::STATE_IDLE,0},{STATE_GENPC,0x10981},{r::STATE_SP,0x1000},
			{r::STATE_IFR,0x8008},{r::STATE_IMR,imr},{r::STATE_ST1,st1}},{{0xfff,0x5a5a}});
		auto final=observation("one-NMI-stack-write-despite-held-input",{{r::STATE_IDLE,1},{STATE_GENPC,0x10008},{r::STATE_SP,0xfff},
			{r::STATE_IFR,0},{r::STATE_IMR,imr},{r::STATE_ST1,0x800}},{{0xfff,0x981}});
		clocked_register_case c{{{0,std::move(initial)},{16,std::move(asleep)},{17,std::move(pending)},
			{21,observation("NMI-vector-entry-running",{})},{80,std::move(final)}},17,80};
		c.initial_input_lines={{INPUT_LINE_NMI,CLEAR_LINE},{INPUT_LINE_HALT,CLEAR_LINE}};
		// HALT is a fixture execution hold, not a claim about a C54x board pin.
		c.actions={{16,{{r::STATE_IFR,0x8008}},false,{},{{INPUT_LINE_HALT,ASSERT_LINE},{INPUT_LINE_NMI,ASSERT_LINE}}},
			{20,{},false,{},{{INPUT_LINE_HALT,CLEAR_LINE}}},
			{48,{},false,{},{{INPUT_LINE_NMI,ASSERT_LINE}}}};
		c.data_events=std::vector<clocked_data_event>{{true,0xfff,0x981,0,false},{true,1,0x8008,0,false}};
		c.data_events->front().registers={{r::STATE_IFR,0x8008}};
		c.irqs={{INPUT_LINE_NMI,0,~0U,0,0x981}};
		c.irqs.front().clear_input_on_ack=false;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
