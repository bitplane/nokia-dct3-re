// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (u16 value : {0x1234U,0x1000U}) for (unsigned point : {4U,7U,8U,9U})
	{
		auto initial = initial_case("store-move-compare-sequence", {0x80e6,0x10,0x71e6,0x11,0x12,0xf4aa});
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,0x608},{r::STATE_ST1,0x800},
			{r::STATE_AR0,0x1000},{r::STATE_AR2,0x1000},{r::STATE_AR6,0x1000},{r::STATE_A,0x33fa},{r::STATE_B,0x1122}});
		initial.data = {{0x1010,0xdead},{0x1011,value}};
		auto final = observation("compared", {{r::STATE_AR2,value},{r::STATE_AR6,0x1000},
			{r::STATE_ST0,value == 0x1000 ? 0x1608U : 0x608U},{r::STATE_A,0x33fa},{r::STATE_B,0x1122}});
		clocked_register_case c{{{0,std::move(initial)},{16,std::move(final)}},point,16};
		c.require_idle = false;
		c.data_events = std::vector<clocked_data_event>{{true,0x1010,0x33fa,7},{false,0x1011,value,8},{true,0x12,value,8}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
