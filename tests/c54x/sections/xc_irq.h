// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant < 4; ++variant)
	{
		auto initial = initial_case("xc-irq", {u16(variant & 1 ? 0xff00 : 0xfd00),0x8000,0x8001});
		for (unsigned i = 0; i < 32; ++i) initial.program.push_back({0x40 + i,0xf495});
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,0xa60e},{r::STATE_ST1,0},
			{r::STATE_A,0x1234},{r::STATE_SP,0x500},{r::STATE_IMR,1}});
		clocked_register_case c{{{0,std::move(initial)}},variant >= 2 ? 8U : 5U,24};
		c.states.push_back({24,observation("protected-stores-retired", {{r::STATE_IFR,0},{r::STATE_SP,0x4ff}},
			{{0x700,0x1234},{0x701,0x1234}})});
		c.actions = {{3,{{r::STATE_IFR,1}}}};
		c.require_idle = false; c.check_pipeline_clock = true;
		c.data_ranges = {{{0x700,0x701}}};
		c.data_events = std::vector<clocked_data_event>{{true,0x700,0x1234,7},{true,0x701,0x1234,8}};
		c.program_events = {{false,0x40,0xf495,11}};
		c.irqs = {{0,9,24,2}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
