// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_HIGH_PROGRAM
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned page_index = 0; page_index < 5; ++page_index)
	for (unsigned source_page : {0U,2U})
	{
		unsigned const target_page = std::array<unsigned,5>{0,1,2,3,0x7f}[page_index];
		u32 const source = source_page << 16, target = target_page << 16;
		auto initial = initial_case("far-branch", {u16(0xf880 | target_page),0x700,0xe811,0xe922});
		for (auto &word : initial.program) word.address += source;
		for (unsigned i = 0; i < 24; ++i) initial.program.push_back({target + 0x700 + i,u16(i ? 0xf495 : 0xe844)});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,source + 0x600},{r::STATE_ST0,0xa608}});
		clocked_register_case c{{{0,std::move(initial)}},
			std::array<unsigned,4>{3,5,7,10}[(page_index + source_page / 2) % 4],14};
		for (unsigned tick = 1; tick <= 14; ++tick) {
			auto state = observation("page-redirect", {{r::STATE_A,tick >= 10 ? 0x44U : 0U},{r::STATE_B,0},
				{r::STATE_SP,0x1000},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x800}});
			// Debugger XPC during redirected Fetch is not an architectural commit claim.
			if (tick != 6) state.expected.push_back({r::STATE_XPC,tick >= 7 ? target_page : source_page});
			c.states.push_back({tick,std::move(state)});
		}
		c.require_idle = false; c.check_pipeline_clock = true;
		c.program_events = {{false,source + 0x601,0x700,3},{false,target + 0x700,0xe844,6}};
		c.data_events = std::vector<clocked_data_event>{};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
