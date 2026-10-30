// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	// Nine-bit DP boundaries and old/new-page alias, not all512 page literals.
	for (u16 page : {0U,1U,7U,8U,255U,256U,510U,511U})
	for (unsigned absolute = 0; absolute < 2; ++absolute) for (unsigned snapshot = 0; snapshot < 3; ++snapshot)
	{
		u16 const target = (page << 7) | 0x7e;
		auto initial = absolute ? initial_case("memory-dp", {0x46f8,0x47f,0x107e}) :
			initial_case("memory-dp", {0x467f,0x107e});
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,0x608},{r::STATE_B,0x123456789aULL}});
		initial.expected.insert(initial.expected.end(), {{r::STATE_ST0,0x608},{r::STATE_ST1,0x800},
			{r::STATE_A,0},{r::STATE_B,0x123456789aULL}});
		initial.data = {{0x47f,0xe000 | page},{0x47e,0x1111},{target,0x2222}};
		auto published = observation("dp-published", {}); published.expected = initial.expected;
		published.expected[1].value = 0x600 | page;
		auto consumed = published; consumed.expected[3].value = 0x2222;
		clocked_register_case c{{{0,std::move(initial)},
			{8 + absolute,std::move(published)},{9 + absolute,std::move(consumed)}},4 + absolute + snapshot,11 + absolute};
		c.require_idle = false; c.check_pipeline_clock = true;
		if (absolute) {
			c.retained_program_reads = {{0x601,0x47f}};
			c.actions.push_back({4,{},false,{{0x601,0xdead}}});
		}
		clocked_register_action poison{5 + absolute,{}};
		poison.data = {{0x47f,0}}; c.actions.push_back(std::move(poison));
		c.data_events = std::vector<clocked_data_event>{{false,0x47f,u16(0xe000 | page),5 + absolute},
			{false,target,0x2222,8 + absolute}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
