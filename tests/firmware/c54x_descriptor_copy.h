// license:BSD-3-Clause
// copyright-holders:Gaz
// Firmware-derived public instruction sequence; no acquired ROM is loaded.
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned sxm = 0; sxm < 2; ++sxm) for (unsigned wrap = 0; wrap < 2; ++wrap)
	for (unsigned point : {8U,9U,10U})
	{
		u16 const value = wrap ? 0xffff : 0x1234, sum = wrap ? 1 : 0x1236;
		auto initial = initial_case("descriptor-copy", {0x6b82,2,0x4b92,0x7182,0x13,0x8b93});
		for (auto &word : initial.program) word.address += 0x23dc5 - 0x600;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x23dc5},{r::STATE_ST0,0x208},
			{r::STATE_ST1,sxm ? 0x900 : 0x800},{r::STATE_AR2,0x470},{r::STATE_AR3,0x620},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0x2233445566ULL}});
		initial.expected.insert(initial.expected.end(), {{r::STATE_SP,0x1000},{r::STATE_AR2,0x470},{r::STATE_AR3,0x620}});
		initial.data = {{0x470,value},{0x471,0x550},{0xfff,0xaaaa},{0x550,0xaaaa}};
		auto allocated = observation("push-allocation", {{r::STATE_SP,0xfff},{r::STATE_AR2,0x471},{r::STATE_AR3,0x620}});
		auto pointer = allocated; pointer.expected[2].value = 0x550;
		auto destination = pointer; destination.expected[2].value = 0x551;
		auto popped = destination; popped.expected[0].value = 0x1000;
		auto final = observation("copied", {{r::STATE_SP,0x1000},{r::STATE_AR2,0x471},{r::STATE_AR3,0x551},
			{r::STATE_ST0,wrap ? sxm ? 0xa08U : 0x608U : 0x208U},{r::STATE_ST1,sxm ? 0x900U : 0x800U},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0x2233445566ULL}}, {{0x550,sum},{0x470,sum}});
		clocked_register_case c{{{0,std::move(initial)},{6,std::move(allocated)},{8,std::move(pointer)},
			{9,std::move(destination)},{10,std::move(popped)},{14,std::move(final)}},point,14};
		c.require_idle = false; c.check_pipeline_clock = true;
		clocked_register_action poison{10,{}}; poison.data = {{0xfff,0xdead}};
		c.actions.push_back(std::move(poison));
		// Same-clock pointer Read precedes push Write in the model; not a silicon timing claim.
		c.data_events = std::vector<clocked_data_event>{{false,0x470,value,6},{true,0x470,sum,7},
			{false,0x471,0x550,8},{true,0x13,0x550,8},{true,0xfff,sum,8},
			{false,0xfff,sum,10},{true,0x550,sum,11}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
