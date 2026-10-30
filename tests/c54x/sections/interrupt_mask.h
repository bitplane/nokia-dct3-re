// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned set = 0; set < 2; ++set) for (unsigned adjacent = 0; adjacent < 2; ++adjacent)
	for (unsigned words = 1; words <= 3; ++words)
	{
		bool const first_sets = bool(set) != bool(adjacent);
		auto initial = initial_case("interrupt-mask-follower", {u16(first_sets ? 0xf7bb : 0xf6bb)});
		if (adjacent) initial.program[1].value = set ? 0xf7bb : 0xf6bb;
		initial.program[1 + adjacent].value = words == 1 ? 0xe842 : words == 2 ? 0xf020 : 0x76f8;
		if (words >= 2) initial.program[2 + adjacent].value = words == 2 ? 0x1234 : 0x2000;
		if (words == 3) initial.program[3 + adjacent].value = 0x1234;
		for (unsigned at = 0x44; at < 0x60; ++at) initial.program.push_back({at,at == 0x44 ? 0xe955U : 0xf495U});
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST1,first_sets ? 0 : 0x800},{r::STATE_IMR,2}});
		initial.data = {{0x2000,0}};
		auto final = observation("protected-completion", {{r::STATE_A,words == 1 ? 0x42U : words == 2 ? 0x1234U : 0U},
			{r::STATE_B,set ? 0U : 0x55U},{r::STATE_IFR,set ? 2U : 0U},{r::STATE_SP,set ? 0x1000U : 0xfffU},
			{r::STATE_ST1,0x800}}, {{0x2000,words == 3 ? 0x1234U : 0U}});
		clocked_register_case c{{{0,std::move(initial)},{20,std::move(final)}},4,20};
		c.require_idle = false; c.check_pipeline_clock = true;
		c.actions = {{3,{{r::STATE_IFR,2}}}};
		c.data_ranges = {{0x2000,0x2000}};
		c.data_events = std::vector<clocked_data_event>{};
		if (words == 3) c.data_events->push_back({true,0x2000,0x1234,0,false});
		if (!set) {
			// SPRU131G INTM latency: complete follower, including all extension
			// words and its memory write, before the vector Fetch/IACK boundary.
			c.irq = clocked_irq_expectation{1,7 + words + adjacent,20,unsigned(words == 3)};
			c.program_events = {{false,0x44,0xe955,0,false}};
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
