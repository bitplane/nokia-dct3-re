// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned kind = 0; kind < 4; ++kind) {
		auto initial = initial_case("block-repeat-control", {0xf495});
		if (kind < 3) {
			initial.program[0].value = 0xf072; initial.program[1].value = kind == 1 ? 0x604 : 0x603;
			initial.program[2].value = 0x6d91;
			initial.program[3].value = kind == 0 ? 0xf6bf : kind == 1 ? 0x7707 : 0x8a07;
			initial.program[4].value = kind == 1 ? 0 : 0xf4e1; initial.program[5].value = 0xf4e1;
			for (unsigned i = 0; i < 4; ++i) initial.data.push_back({0x1000 + i,0});
		} else {
			initial.program[0].value = 0x7707; initial.program[1].value = 0x8000;
			initial.program[2].value = 0x6d91; initial.program[4].value = 0xf4e1;
			initial.initial.insert(initial.initial.end(), {{r::STATE_RSA,0x602},{r::STATE_REA,0x603}});
		}
		initial.initial.insert(initial.initial.end(), {{r::STATE_BRC,3},{r::STATE_AR1,0},{r::STATE_ST1,0}});
		clocked_register_case c{{{0,std::move(initial)}},kind & 1 ? 6U : 1U,64};
		c.pipeline_frontend = false;
		c.states.push_back({64,observation("architectural-braf-control", {{r::STATE_AR1,kind == 3 ? 4U : 1U},
			{r::STATE_BRC,kind == 3 ? 0U : 3U},{r::STATE_SP,kind == 2 ? 0x1001U : 0x1000U},
			{r::STATE_ST1,0,0x8000}})});
		cases.push_back(std::move(c));
	}
	for (unsigned extended = 0; extended < 2; ++extended)
	for (unsigned count : {0U,2U}) {
		unsigned const body = 0x603 + extended;
		auto initial = initial_case("nested-repeat", {0xf072,u16(body),u16(extended ? 0xf070 : 0xec00 | count)});
		if (extended) initial.program[3].value = count;
		initial.program[body - 0x600].value = 0x6d91;
		initial.program[body - 0x600 + 1].value = 0xf4e1;
		initial.initial.insert(initial.initial.end(), {{r::STATE_BRC,3},{r::STATE_AR1,0},{r::STATE_ST1,0}});
		clocked_register_case c{{{0,std::move(initial)}},extended ? 6U : 1U,96};
		c.pipeline_frontend = false;
		c.states.push_back({96,observation("nested-loop-counts", {{r::STATE_AR1,4 * (count + 1)},
			{r::STATE_BRC,0},{r::STATE_ST1,0,0x8000}})});
		c.data_events = std::vector<clocked_data_event>{};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
