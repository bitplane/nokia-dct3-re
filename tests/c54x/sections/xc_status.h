// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned staged = 0; staged < 2; ++staged)
	for (unsigned gap = 0; gap < 4; ++gap)
	for (unsigned set = 0; set < 2; ++set)
	for (unsigned old = 0; old < 2; ++old)
	for (unsigned condition = 0; condition < 2; ++condition)
	{
		bool const take = (gap < 2 ? old : set) == condition;
		auto initial = initial_case("xc-status", {u16(set ? 0xf5bc : 0xf4bc)});
		initial.program[1 + gap].value = condition ? 0xfd30 : 0xfd20;
		initial.program[2 + gap].value = 0xe833;
		initial.program[4 + gap].value = 0xf4e1;
		initial.initial.push_back({r::STATE_ST0,0xe05 | (old ? 0x1000U : 0U)});
		clocked_register_case c{{{0,std::move(initial)}},(gap + set + old + condition) & 1 ? 6U : 3U,32};
		c.pipeline_frontend = staged;
		c.states.push_back({32,observation("access-sampled-status", {{r::STATE_A,take ? 0x33U : 0U},{r::STATE_B,0},
			{STATE_GENPC,0x605 + gap},{r::STATE_ST0,0xe05 | (set ? 0x1000U : 0U)},
			{r::STATE_ST1,0x800},{r::STATE_SP,0x1000}})});
		c.data_events = std::vector<clocked_data_event>{};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
