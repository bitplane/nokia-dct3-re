// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant < 16; ++variant)
	{
		bool const pending = variant >= 8, delayed = !(variant & 1);
		u32 const base = variant & 2 ? 0x10000 : 0;
		unsigned const acceptance = delayed ? 9 : 7, finish = pending ? acceptance + 7 : 10;
		auto initial = initial_case("branch-delay", {u16(delayed ? 0xf273 : 0xf073),0x700,0xe822,0xe933,0x77e1});
		for (auto &word : initial.program) word.address += base;
		for (unsigned at = 0x700; at < 0x718; ++at) initial.program.push_back({base + at,at == 0x700 ? 0xe844U : 0xf495U});
		if (pending) for (unsigned at = 0x40; at < 0x4c; ++at) initial.program.push_back({base + at,at == 0x40 ? 0xe955U : 0xf495U});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST1,pending ? 0 : 0x800},{r::STATE_IMR,1}});
		unsigned const point = pending ? variant & 4 ? 8 : 6 : variant & 4 ? 5 : 3;
		clocked_register_case c{{{0,std::move(initial)}},point,finish};
		for (unsigned tick = 1; tick <= finish; ++tick)
		{
			auto state = observation("redirect-and-drain", {{r::STATE_A,tick >= 10 ? 0x44U : delayed && tick >= 8 ? 0x22U : 0U},
				{r::STATE_B,pending && tick >= acceptance + 7 ? 0x55U : delayed && tick >= 9 ? 0x33U : 0U},
				{r::STATE_SP,pending && tick >= acceptance + 2 ? 0xfffU : 0x1000U},
				{r::STATE_ST1,!pending || tick >= acceptance + 3 ? 0x800U : 0U}});
			if (pending && tick > 5) state.expected.push_back({r::STATE_IFR,tick < acceptance + 3 ? 1U : 0U});
			c.states.push_back({tick,std::move(state)});
		}
		c.require_idle = false; c.check_pipeline_clock = true;
		c.program_events = {{false,base + 0x601,0x700,3},{false,base + 0x700,0xe844,6}};
		c.data_events = std::vector<clocked_data_event>{};
		if (pending) {
			u16 const continuation = delayed ? 0x703 : 0x701;
			c.actions = {{5,{},false,{},{{0,ASSERT_LINE},{0,CLEAR_LINE}}}};
			c.irqs = {{0,acceptance + 3,acceptance + 3,0,continuation}};
			c.data_events->push_back({true,0xfff,continuation,acceptance + 4});
			c.program_events.push_back({false,base + 0x40,0xe955,acceptance + 3});
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
