// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index=0;index<6;++index) {
		bool const timer=index>=4, enabled=index&1, serviced=enabled && index<2, continued=enabled && !serviced;
		u16 const imr=enabled ? timer ? 8 : 1 : 0, st1=index>=2 ? 0x800 : 0;
		// Observe the pending flag at the continuation write, then acknowledge
		// it before the terminal IDLE; do not conflate wake with pending re-entry.
		auto initial=initial_case("IDLE1-mask-gates-wake-independently-of-INTM",{0xf4e1,0x76f8,0x501,1,0x7726,0x10,u16(0xe800|imr),0x8801,0xf4e1});
		for (auto &word : initial.program) word.address+=0x10300;
		initial.program.push_back({0x10040,0xf4e1});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,0x10900},{r::STATE_ST1,st1},{r::STATE_IMR,imr},{r::STATE_SP,0x1000}});
		initial.data={{0x501,0},{0xfff,0x5a5a}};
		auto asleep=observation("no-foreground-before-wake-stimulus",{{r::STATE_IDLE,1},{STATE_GENPC,0x10901},{r::STATE_SP,0x1000}},{{0x501,0}});
		auto final=observation("wake-selects-vector-foreground-or-remains-idle",{{r::STATE_IDLE,1},
			{STATE_GENPC,serviced ? 0x10041U : continued ? 0x10909U : 0x10901U},
			{r::STATE_SP,serviced ? 0xfffU : 0x1000U},{r::STATE_IMR,imr},{r::STATE_ST1,serviced ? 0x800U : st1},
			{r::STATE_IFR,serviced || continued ? 0U : timer ? 8U : 1U}},{{0x501,u16(continued ? 1 : 0)},{0xfff,u16(serviced ? 0x901 : 0x5a5a)}});
		clocked_register_case c{{{0,std::move(initial)},{16,std::move(asleep)},
			{17,observation("wake-source-pending",{})},{128,std::move(final)}},12,128};
		c.initial_input_lines={{0,CLEAR_LINE}};
		if (timer) c.actions={{16,{{r::STATE_PRD,9},{r::STATE_TCR,0x20}}}};
		else c.actions={{16,{},false,{},{{0,ASSERT_LINE},{0,CLEAR_LINE}}}};
		c.data_events=std::vector<clocked_data_event>{};
		if (continued) {
			*c.data_events={{true,0x501,1,0,false},{true,0x26,0x10,0,false},{true,1,imr,0,false}};
			c.data_events->front().registers={{r::STATE_IFR,imr}};
		}
		if (serviced) {
			c.data_events->push_back({true,0xfff,0x901,0,false});
			c.irqs={{0,0,~0U,0,0x901}};
		}
		cases.push_back(std::move(c));
	}
	for (unsigned checkpoint : {18U,22U,202U,206U}) {
		auto initial=initial_case("two-interrupts-through-backward-IDLE-loop",{0xf4e1,0xf073,0x600});
		initial.program.insert(initial.program.end(),{{0x40,0xf300},{0x41,1},{0x42,0xf4eb}});
		initial.initial.insert(initial.initial.end(),{{r::STATE_ST1,0},{r::STATE_IMR,1},{r::STATE_SP,0x1000},{r::STATE_B,0}});
		auto final=observation("both-receives-return-to-idle-with-no-pending-guard",{{r::STATE_IDLE,1},{r::STATE_ILLEGAL,0},
			{r::STATE_SP,0x1000},{r::STATE_B,2},{r::STATE_IFR,0},{r::STATE_RTN,0x601}});
		clocked_register_case c{{{0,std::move(initial)},{400,std::move(final)}},checkpoint,400};
		c.initial_input_lines={{0,CLEAR_LINE}};
		c.actions={{16,{},false,{},{{0,ASSERT_LINE},{0,CLEAR_LINE}}},
			{200,{},false,{},{{0,ASSERT_LINE},{0,CLEAR_LINE}}}};
		c.data_events=std::vector<clocked_data_event>{{true,0xfff,0x601,0,false},{false,0xfff,0x601,0,false},
			{true,0xfff,0x601,0,false},{false,0xfff,0x601,0,false}};
		c.irqs={{0,0,~0U,0,0x601},{0,0,~0U,2,0x601}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
