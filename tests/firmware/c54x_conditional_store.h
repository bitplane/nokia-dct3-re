// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	// R060 02:3dcf sequence: a conditional store retains its destination
	// independently of the following long MAR; both predicate outcomes matter.
	for (unsigned selected = 0; selected < 2; ++selected) for (unsigned point : {5U,9U,11U})
	{
		u16 const st0 = selected ? 0x9608 : 0x8608;
		auto initial = initial_case("conditional-store-mar", {0x108a,0x6d8a,0xfd30,0x8082,0x6dea,3});
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,st0},{r::STATE_ST1,0x800},{r::STATE_A,0x1234},{r::STATE_AR2,0x470}});
		initial.data = {{0x470,0xcafe},{0x46e,0x7777}};
		clocked_register_case c{{{0,std::move(initial)},
			{4,observation("first-decrement", {{r::STATE_AR2,0x46f},{r::STATE_A,0x1234},{r::STATE_ST0,st0}})},
			{5,observation("second-decrement", {{r::STATE_AR2,0x46e},{r::STATE_A,0x1234},{r::STATE_ST0,st0}})},
			{6,observation("loaded", {{r::STATE_AR2,0x46e},{r::STATE_A,0xcafe},{r::STATE_ST0,st0}})},
			{9,observation("long-modification", {{r::STATE_AR2,0x471},{r::STATE_A,0xcafe},{r::STATE_ST0,st0},
				{r::STATE_ST1,0x800},{r::STATE_SP,0x1000}}, {{0x46e,selected ? 0xcafeU : 0x7777U}})}},point,12};
		c.require_idle = false; c.check_pipeline_clock = true;
		c.program_events = {{false,0x605,3,7}};
		c.data_events = std::vector<clocked_data_event>{{false,0x470,0xcafe,5}};
		if (selected) c.data_events->push_back({true,0x46e,0xcafe,9});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
