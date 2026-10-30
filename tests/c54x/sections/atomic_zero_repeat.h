// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned form = 0; form != 3; ++form) {
		auto initial = initial_case("zero-count-repeat-executes-body-once", {u16(form == 0 ? 0xf071 : form == 1 ? 0xf272 : 0xf072),
			u16(form == 0 ? 0 : form == 1 ? 0x604 : 0x602),0xf495,0xf495,0xf495,0xf4e1});
		if (form != 1) initial.program[3].value = 0xf4e1;
		initial.program.push_back({0x5ff,0xf495});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_A,0x123456789a},{r::STATE_BRC,0}});
		auto final = observation("one-body-and-exact-continuation", {{STATE_GENPC,form == 1 ? 0x606U : 0x604U},
			{r::STATE_A,form ? 0x123456789aULL : 0ULL},{r::STATE_ST1,0,0x8000}});
		if (!form) final.expected.push_back({r::STATE_RC,0});
		else final.expected.push_back({r::STATE_BRC,0});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-repeat-snapshot", {{STATE_GENPC,0x600}})},
			{2,observation("repeat-running", {})},{32,std::move(final)}},1,32};
		c.pipeline_frontend = false; c.data_events = std::vector<clocked_data_event>{};
		c.program_events = {{false,form == 1 ? 0x604U : 0x602U,0xf495,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
