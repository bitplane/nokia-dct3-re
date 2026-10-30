// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned form = 0; form < 3; ++form) for (unsigned combine = 0; combine < 2; ++combine)
	for (unsigned after_read = 0; after_read < 2; ++after_read)
	{
		bool const absolute = form == 1;
		unsigned const read = absolute ? 7 : 6, write = read + 1, isr = write + 4;
		u16 const result = combine ? 0xfdbb : 0x4022, continuation = absolute ? 0x603 : 0x602;
		auto initial = initial_case("interrupt-rmw", {u16((combine ? 0x6900 : 0x6800) | (absolute ? 0xf8 : form == 2 ? 0x91 : 0x20)),
			u16(absolute ? 0x420 : 0xe833)});
		if (absolute) initial.program[2].value = 0xe833;
		for (unsigned at = 0x40; at < 0x50; ++at) initial.program.push_back({at,at == 0x40 ? 0xe955U : 0xf495U});
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,8},{r::STATE_ST1,0},{r::STATE_IMR,1},{r::STATE_AR1,0x420}});
		initial.data = {{0x420,0x55aa},{0x4a0,0xbeef}};
		initial.expected.insert(initial.expected.end(), {{r::STATE_A,0},{r::STATE_B,0}});
		initial.expected_data = {{0x4a0,0xbeef}};
		auto final = observation("retired-before-isr", {{r::STATE_A,0},{r::STATE_B,0x55},{r::STATE_SP,0xfff},{r::STATE_AR1,0x4a0},
			{r::STATE_IFR,0}}, {{0x420,result},{0x4a0,0xbeef},{0xfff,continuation}});
		clocked_register_case c{{{0,std::move(initial)},
			{write,observation("rmw-retired", {{r::STATE_A,0},{r::STATE_B,0}}, {{0x420,result},{0x4a0,0xbeef}})},
			{isr,std::move(final)}},after_read ? read : absolute ? 4U : 3U,isr};
		if (form == 2) {
			c.states.insert(c.states.begin() + 1,{read - 1,observation("source-postincrement", {{r::STATE_A,0},{r::STATE_B,0},{r::STATE_AR1,0x421}})});
			c.states.insert(c.states.begin() + 2,{read,observation("pointer-mutated", {{r::STATE_A,0},{r::STATE_B,0},{r::STATE_AR1,0x4a0}})});
		}
		c.require_idle = false; c.check_pipeline_clock = true;
		c.actions = {{2,{},false,{},{{0,ASSERT_LINE},{0,CLEAR_LINE}}},
			{read - 1,{{r::STATE_ST0,9},{r::STATE_AR1,0x4a0}}},{read,{}}};
		c.actions.back().data = {{0x420,0x3333}};
		c.program_events = {{false,0x601,u16(absolute ? 0x420 : 0xe833),3}};
		if (absolute) c.program_events.push_back({false,0x602,0xe833,4});
		c.program_events.push_back({false,0x40,0xe955,0,false});
		c.data_events = std::vector<clocked_data_event>{{false,0x420,0x55aa,read},{true,0x420,result,write},
			{true,0xfff,continuation,0,false}};
		// IACK precedes older Execute's Write tap in the same model clock;
		// the RMW write still precedes stack publication and ISR execution.
		c.irq = clocked_irq_expectation{0,write,isr,1,continuation};
		cases.push_back(std::move(c));
	}
	{
		auto initial = initial_case("interrupt-mask-rmw", {0x68f8,0,0xfffe});
		for (unsigned at = 0x40; at < 0x60; ++at) initial.program.push_back({at,at == 0x40 ? 0xe955U : 0xf495U});
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST1,0},{r::STATE_IMR,1}});
		initial.expected.insert(initial.expected.end(), {{r::STATE_IMR,1},{r::STATE_B,0}});
		clocked_register_case c{{{0,std::move(initial)},
			{8,observation("mask-cleared", {{r::STATE_IMR,0},{r::STATE_B,0}})},
			{12,observation("accepted-isr", {{r::STATE_IMR,0},{r::STATE_B,0x55},{r::STATE_SP,0xfff}})},
			{18,observation("new-request-masked", {{r::STATE_IMR,0},{r::STATE_IFR,1},{r::STATE_ST1,0},
				{r::STATE_B,0x55},{r::STATE_SP,0xfff}}, {{0xfff,0x603}})}},7,18};
		c.require_idle = false; c.check_pipeline_clock = true;
		c.actions = {{2,{},false,{},{{0,ASSERT_LINE},{0,CLEAR_LINE}}},
			{12,{{r::STATE_ST1,0}},false,{},{{0,ASSERT_LINE},{0,CLEAR_LINE}}}};
		c.program_events = {{false,0x601,0,3},{false,0x602,0xfffe,4},{false,0x40,0xe955,0,false}};
		c.data_events = std::vector<clocked_data_event>{{false,0,1,7},{true,0,0,8},{true,0xfff,0x603,0,false}};
		c.irq = clocked_irq_expectation{0,8,8,1,0x603};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
