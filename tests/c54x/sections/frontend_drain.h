// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 3; ++index) {
		bool const memory = index != 0, b = index == 2;
		auto initial = initial_case("foreground-drain-IRQ", {u16(memory ? b ? 0x1120 : 0x1020 : 0xe811),0xe822,0xe833});
		for (unsigned at = 0x40; at < 0x60; ++at) initial.program.push_back({at,u16(at == 0x40 ? 0xe955 : 0xf495)});
		initial.data = {{0xfff,0xbeef},{0x420,0x1234}};
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,8},{r::STATE_ST1,0},{r::STATE_IMR,1}});
		clocked_register_case c{{{0,std::move(initial)}},memory ? 5U : 4U,10};
		for (unsigned tick = 1; tick <= 10; ++tick) c.states.push_back({tick,observation("decoded-retires-flushed-does-not", {
			{r::STATE_A,tick < 6 || b ? 0U : memory ? 0x1234U : 0x11U},
			{r::STATE_B,tick >= 10 ? 0x55U : tick >= 6 && b ? 0x1234U : 0U},
			{r::STATE_SP,tick < 5 ? 0x1000U : 0xfffU}},{{0xfff,u16(tick < 7 ? 0xbeef : 0x601)}})});
		c.actions = {{2,{},false,{},{{0,ASSERT_LINE}}}};
		if (memory) c.actions.push_back({5,{},false,{}, {},{{0x420,0x5678}}});
		c.data_events = std::vector<clocked_data_event>{};
		if (memory) c.data_events->push_back({false,0x420,0x1234,5});
		c.data_events->push_back({true,0xfff,0x601,7});
		c.program_events = {{false,0x602,0xe833,4},{false,0x40,0xe955,6}};
		c.irqs = {{0,6,6,unsigned(memory),0x601}};
		c.require_idle = false; c.check_pipeline_clock = true;
		cases.push_back(std::move(c));
	}
	auto initial = initial_case("unsupported-staged-decode", {0x77e1,0xf495});
	initial.initial.push_back({r::STATE_AR1,0});
	initial.expected.clear();
	clocked_register_case unsupported{{{0,std::move(initial)}},1,3};
	auto rejected = observation("no-atomic-fallback", {{r::STATE_ILLEGAL,1},{r::STATE_AR1,0}});
	rejected.expected.pop_back();
	unsupported.states.push_back({3,std::move(rejected)});
	unsupported.require_idle = false;
	unsupported.forbid_data_access = true;
	cases.push_back(std::move(unsupported));
	return cases;
}
}
