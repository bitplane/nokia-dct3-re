// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned staged = 0; staged < 2; ++staged)
	for (unsigned kind = 0; kind < 3; ++kind)
	for (unsigned initial_tc = 0; initial_tc < 2; ++initial_tc)
	{
		bool const taken = kind != 2;
		auto initial = initial_case("return-barrier", {u16(initial_tc ? 0xf4bc : 0xf5bc),
			u16(kind == 0 ? 0xfc00 : kind == 1 ? 0xfc45 : 0xfc44),u16(initial_tc ? 0xfd20 : 0xfd30),
			0xe933,0xf495,0xf4e1});
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,0xe05 | (initial_tc ? 0x1000U : 0U)}});
		initial.data = {{0x1000,0x602}};
		clocked_register_case c{{{0,std::move(initial)}},(kind + initial_tc) & 1 ? 9U : 3U,32};
		c.pipeline_frontend = staged;
		c.states.push_back({32,observation("settled-status-after-return", {{r::STATE_A,0},{r::STATE_B,0x33},
			{STATE_GENPC,0x606},{r::STATE_SP,0x1000 + unsigned(taken)},
			{r::STATE_ST0,0xe05 | (!initial_tc ? 0x1000U : 0U)}})});
		c.data_events = std::vector<clocked_data_event>{};
		if (taken) c.data_events->push_back({false,0x1000,0x602,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
