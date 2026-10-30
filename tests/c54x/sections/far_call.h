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
		auto initial = initial_case("far-call", {u16(0xf980 | target_page),0x700,0xe844,0xe811});
		for (auto &word : initial.program) word.address += source;
		for (unsigned i = 0; i < 24; ++i) initial.program.push_back({target + 0x700 + i,
			u16(i == 0 ? 0xe955 : i == 1 ? 0xf4e4 : i == 2 ? 0xffff : 0xf495)});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,source + 0x600},{r::STATE_ST0,0xa608},{r::STATE_RTN,0xbeef}});
		clocked_register_case c{{{0,std::move(initial)}},
			std::array<unsigned,8>{3,4,5,6,7,10,11,12}[(2 * page_index + source_page / 2) % 8],20};
		for (unsigned tick = 1; tick <= 20; ++tick) {
			auto state = observation("two-word-call-frame", {
				{r::STATE_SP,tick >= 11 ? 0x1000U : tick >= 10 ? 0xfffU : tick >= 5 ? 0xffeU : tick >= 4 ? 0xfffU : 0x1000U},
				{r::STATE_A,tick >= 18 ? 0x11U : tick >= 17 ? 0x44U : 0U},{r::STATE_B,tick >= 10 ? 0x55U : 0U},
				{r::STATE_ST0,0xa608},{r::STATE_ST1,0x800},{r::STATE_RTN,0xbeef}});
			if (tick >= 12) state.expected.push_back({r::STATE_XPC,source_page});
			c.states.push_back({tick,std::move(state)});
		}
		c.actions = {{10,{},false,{}, {},{{0xffe,0x7fff}}},{11,{},false,{}, {},{{0xfff,0x7fff}}}};
		c.require_idle = false; c.check_pipeline_clock = true;
		c.program_events = {{false,source + 0x601,0x700,3},{false,source + 0x602,0xe844,4},
			{false,target + 0x700,0xe955,6},{false,source + 0x602,0xe844,13}};
		c.data_events = std::vector<clocked_data_event>{{true,0xfff,0x602,6},{true,0xffe,u16(source_page),7},
			{false,0xffe,u16(source_page),10},{false,0xfff,0x602,11}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
