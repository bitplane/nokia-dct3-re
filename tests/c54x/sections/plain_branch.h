// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant < 16; ++variant)
	{
		bool const b = variant & 1, equal = variant & 2, nonzero = variant & 4;
		bool const taken = equal ? !nonzero : nonzero;
		u32 const base = variant & 8 ? 0x10000 : 0;
		u64 const value = nonzero ? 0x0100000000ULL : 0;
		auto initial = initial_case("plain-branch", {u16(0xf844 | (unsigned(b) << 3) | unsigned(equal)),
			0x700,0x1070,0x1071,0xe866});
		for (auto &word : initial.program) word.address += base;
		for (unsigned i = 0; i < 24; ++i) initial.program.push_back({base + 0x700 + i,u16(i ? 0xf495 : 0xe855)});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xa608},
			{r::STATE_A,b ? 0x9999 : value},{r::STATE_B,b ? value : 0x7777}});
		initial.data = {{0x470,0x1111},{0x471,0x2222}};
		clocked_register_case c{{{0,std::move(initial)}},4 + variant % 3,14};
		c.actions = {{6,{{int(b ? r::STATE_B : r::STATE_A),nonzero ? 0U : 1U}}}};
		c.states.push_back({11,observation("retained-branch-outcome", {{r::STATE_A,taken ? 0x55U : 0x66U}})});
		c.require_idle = false; c.check_pipeline_clock = true;
		c.program_events = {{false,base + 0x602,0x1070,4},{false,base + 0x603,0x1071,6}};
		if (taken) c.program_events.push_back({false,base + 0x700,0xe855,7});
		c.data_events = std::vector<clocked_data_event>{};
		if (!taken) {
			c.data_events->push_back({false,0x470,0x1111,8});
			c.data_events->push_back({false,0x471,0x2222,9});
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
