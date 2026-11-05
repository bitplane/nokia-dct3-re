// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_DEEP_IDLE_CLOCKS
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index=0;index<8;++index) {
		bool const external=index>=4, enabled=index&1;
		u16 const idle=(index%4)<2 ? 0xf6e1 : 0xf5e1;
		u16 const imr=enabled ? external ? 0x20 : 1 : 0;
		// Preserve wake flags, but mask them in firmware before the terminal IDLE.
		auto initial=initial_case("deep-idle-internal-versus-external-clock",{idle,0x76f8,0x501,1,0xe800,0x8800,0xf4e1});
		for (auto &word : initial.program) word.address+=0x10320;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,0x10920},{r::STATE_ST1,0x800},
			{r::STATE_IMR,imr},{r::STATE_PRD,99},{r::STATE_TCR,u16(external ? 0x10 : 0x23)}});
		initial.data={{0x501,0}};
		auto asleep=observation("timer-and-serial-await-eligible-clock",{{r::STATE_IDLE,1},
			{STATE_GENPC,0x10921},{r::STATE_IFR,0},{r::STATE_IMR,imr}},{{0x501,0}});
		auto final=observation("masked-source-stays-asleep-enabled-source-continues",{{r::STATE_IDLE,1},
			{STATE_GENPC,enabled ? 0x10927U : 0x10921U},{r::STATE_IMR,0},{r::STATE_ST1,0x800},
			{r::STATE_IFR,external ? 0x20U : enabled ? 9U : 1U}},{{0x501,u16(enabled)}});
		clocked_register_case c{{{0,std::move(initial)},{100,std::move(asleep)},
			{2601,observation("clock-stimulus-issued",{})},{3900,std::move(final)}},1300,3900};
		c.initial_input_lines={{0,CLEAR_LINE},{5,CLEAR_LINE}};
		if (!external) c.actions={{2600,{},false,{},{{0,ASSERT_LINE},{0,CLEAR_LINE}}}};
		c.data_events=std::vector<clocked_data_event>{};
		if (enabled) {
			c.data_events->push_back({true,0x501,1,0,false});
			c.data_events->front().registers={{r::STATE_IMR,imr}};
			c.data_events->push_back({true,0,0,0,false});
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
