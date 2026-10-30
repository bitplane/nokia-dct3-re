// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	// Six-word instruction excerpt from R060 aa22 02:3dd5..3dda,
	// relocated with synthetic data. No private ROM image is required.
	for (unsigned checkpoint : {6U,7U,8U,10U,11U}) {
		auto initial = initial_case("logic-composition", {0xf000,4,0x1892,0x1a8a,0x6d8a,0x8082});
		for (auto &word : initial.program) word.address += 0x20000;
		initial.data = {{0x470,0xff0f},{0x471,0xf0},{0x46f,0x7777}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x20600},{r::STATE_ST0,0xa600},{r::STATE_ST1,0x800},
			{r::STATE_A,0x1234},{r::STATE_B,0x2233445566ULL},{r::STATE_AR2,0x470}});
		clocked_register_case c{{{0,std::move(initial)}},checkpoint,14};
		for (unsigned tick = 1; tick <= 14; ++tick) c.states.push_back({tick,observation("composed-retained-operands", {
			{r::STATE_A,tick >= 9 ? 0x12f8U : tick >= 8 ? 0x1208U : tick >= 7 ? 0x1238U : 0x1234U},
			{r::STATE_AR2,tick > 10 ? 0x650U : 0x470 + unsigned(tick >= 6) - unsigned(tick >= 7) - unsigned(tick >= 8)},
			{r::STATE_B,0x2233445566ULL},{r::STATE_ST0,0xa600},{r::STATE_ST1,0x800},{r::STATE_SP,0x1000}})});
		c.actions = {{7,{},false,{}, {},{{0x470,0xdead}}},{8,{},false,{}, {},{{0x471,0xdead}}},
			{10,{{r::STATE_AR2,0x650}},false}};
		c.data_events = std::vector<clocked_data_event>{{false,0x470,0xff0f,7},{false,0x471,0xf0,8},{true,0x46f,0x12f8,11}};
		c.require_idle = false; c.check_pipeline_clock = true;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
