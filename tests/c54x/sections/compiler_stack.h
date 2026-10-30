// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	// SPRU131G table7-9 Category I: safe separations, including the extra
	// preceding DAGEN-bank conflict. Short-gap old-SP projections are retired.
	for (unsigned producer = 0; producer < 5; ++producer) for (unsigned store = 0; store < 2; ++store)
	for (unsigned replay = 0; replay < 4; ++replay)
	{
		unsigned const prefix = producer == 1 || producer == 2 || producer == 4 ? 2 : 1;
		unsigned const gap = std::array<unsigned,5>{2,1,1,1,3}[producer];
		unsigned const access = 4 + prefix + gap, publish = producer == 3 ? 4 : producer == 4 ? 7 : 5;
		auto initial = initial_case("compiler-stack-producer", {u16(std::array<u16,5>{0x8a18,0x7718,0x7182,0xee20,0x8081}[producer])});
		if (producer == 1 || producer == 2) initial.program[1].value = producer == 1 ? 0x1020 : 0x18;
		if (producer == 4) initial.program[1].value = 0x8a18;
		initial.program[prefix + gap].value = store ? 0x8020 : 0x1020;
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,0xa600},{r::STATE_ST1,0x4800},
			{r::STATE_SP,0x1000},{r::STATE_A,0x7777},{r::STATE_B,0x2233445566ULL},
			{r::STATE_AR1,0x11},{r::STATE_AR2,0x70}});
		initial.data = {{0x70,0x1020},{0x1000,0x1020},{0x1020,0x1111},{0x1040,0x2222}};
		initial.expected.insert(initial.expected.end(), {{r::STATE_SP,0x1000},{r::STATE_A,0x7777},
			{r::STATE_B,0x2233445566ULL},{r::STATE_ST0,0xa600},{r::STATE_ST1,0x4800}});
		auto published = observation("sp-published", {{r::STATE_SP,0x1020},{r::STATE_A,0x7777},
			{r::STATE_B,0x2233445566ULL},{r::STATE_ST0,0xa600},{r::STATE_ST1,0x4800}});
		auto final = published;
		if (!store) for (auto &expected : final.expected)
			if (expected.index == r::STATE_A) expected.value = 0x2222;
		final.expected_data = {{0x1040,store ? 0x7777U : 0x2222U},{0x1020,0x1111}};
		clocked_register_case c{{{0,std::move(initial)},{publish,std::move(published)},
			{access + 2,std::move(final)}},std::array<unsigned,4>{3,publish,access,access + 1}[replay],14};
		if (producer == 4) c.states.insert(c.states.begin() + 1,
			{6,observation("stack-access", {{r::STATE_SP,0x1001},{r::STATE_A,0x7777}})});
		c.require_idle = false; c.check_pipeline_clock = true;
		c.data_events = std::vector<clocked_data_event>{};
		if (producer == 0 || producer == 2 || producer == 4) {
			unsigned const at = producer == 4 ? 6 : 5;
			u16 const source = producer == 2 ? 0x70 : 0x1000;
			c.data_events->push_back({false,source,0x1020,at});
			c.actions = {{at,{}}}; c.actions.front().data = {{source,0xdead}};
		}
		if (producer == 4) c.data_events->push_back({true,0x11,0x7777,6});
		if (producer != 3) c.data_events->push_back({true,0x18,0x1020,publish});
		c.data_events->push_back({bool(store),0x1040,u16(store ? 0x7777 : 0x2222),access + (store ? 2 : 1)});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
