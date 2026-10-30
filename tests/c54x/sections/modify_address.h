// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned form = 0; form < 6; ++form) for (unsigned ar = 0; ar < 8; ++ar)
	for (unsigned compatibility = 0; compatibility < 2; ++compatibility)
	{
		unsigned const selected = compatibility && !ar ? 4 : ar, access = form >= 3 ? 5 : 4;
		s16 const step = std::array<s16,6>{0,-1,1,3,-1,-32768}[form];
		u16 const address = 0x400 + selected * 0x10 + step;
		u16 const st0 = compatibility && ar ? 0x608 | (ar << 13) : 0x8608;
		auto initial = initial_case("modify-address", {u16(0x6d00 | (form >= 3 ? 0xe8 : form == 2 ? 0x90 : form == 1 ? 0x88 : 0x80) | ar)});
		if (form >= 3) initial.program[1].value = u16(step);
		initial.program[form >= 3 ? 2 : 1].value = 0x1080 | ar;
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,0x8608},{r::STATE_ST1,compatibility ? 0x820 : 0x800},{r::STATE_A,0x1234}});
		for (unsigned i = 0; i < 8; ++i) initial.initial.push_back({int(r::STATE_AR0) + int(i),0x400 + i * 16});
		initial.data = {{address,0xcafe}};
		initial.expected.insert(initial.expected.end(), {{int(r::STATE_AR0) + int(selected),0x400 + selected * 16},{r::STATE_A,0x1234},{r::STATE_ST0,0x8608}});
		auto final = observation("loaded-retained-address", {{r::STATE_A,0xcafe},{r::STATE_ST0,st0},
			{r::STATE_ST1,compatibility ? 0x820U : 0x800U},{r::STATE_SP,0x1000}});
		for (unsigned i = 0; i < 8; ++i) final.expected.push_back({int(r::STATE_AR0) + int(i),i == selected ? 0x650U : 0x400 + i * 16});
		clocked_register_case c{{{0,std::move(initial)},
			{access,observation("modified", {{int(r::STATE_AR0) + int(selected),address},{r::STATE_A,0x1234},{r::STATE_ST0,st0}})},
			{access + 2,observation("poisoned-pointer", {{int(r::STATE_AR0) + int(selected),0x650},{r::STATE_A,0x1234}})},
			{access + 3,std::move(final)}},
			(form >= 3 ? std::array<unsigned,3>{3,5,7} : std::array<unsigned,3>{3,4,6})[(form + ar + compatibility) % 3],12};
		c.require_idle = false; c.check_pipeline_clock = true;
		c.actions = {{access + 1,{{int(r::STATE_AR0) + int(selected),0x650}}}};
		c.data_events = std::vector<clocked_data_event>{{false,address,0xcafe,access + 2}};
		if (form >= 3) c.program_events = {{false,0x601,u16(step),3}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
