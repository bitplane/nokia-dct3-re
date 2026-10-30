// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant < 16; ++variant)
	{
		bool const pending = variant >= 12, multiword = variant >= 8 && !pending;
		bool const delayed = multiword || !(variant & 1);
		u32 const base = variant & 2 ? 0x10000 : 0;
		unsigned const acceptance = delayed ? 12 : 10, finish = pending ? acceptance + 4 : 13;
		u16 const opcode = delayed ? 0xf69b : 0xf49b;
		auto initial = initial_case("fast-return", {0xe811,0xe844});
		for (auto &word : initial.program) word.address += base;
		for (unsigned at = 0x40; at < 0x60; ++at) initial.program.push_back({base + at,0xf495});
		initial.program.push_back({base + 0x40,opcode});
		initial.program.push_back({base + 0x41,multiword ? 0xf020U : 0xe822U});
		initial.program.push_back({base + 0x42,multiword ? 0x1234U : 0xe933U});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST1,0},{r::STATE_IMR,1}});
		unsigned const point = pending ? 11 : multiword ? variant & 1 ? 9 : 8 : variant & 4 ? 8 : 7;
		clocked_register_case c{{{0,std::move(initial)}},point,finish};
		for (unsigned tick = 1; tick <= finish; ++tick)
		{
			u64 const a = tick < 6 ? 0 : tick >= 13 ? 0x44 : multiword ? tick >= 12 ? 0x1234 : 0x11 : delayed && tick >= 11 ? 0x22 : 0x11;
			bool const stacked = (tick >= 5 && tick < 8) || (pending && tick >= acceptance + 2);
			bool const masked = (tick >= 6 && tick < 10) || (pending && tick >= acceptance + 3);
			auto state = observation("return-order", {{r::STATE_A,a},{r::STATE_B,!multiword && delayed && tick >= 12 ? 0x33U : 0U},
				{r::STATE_SP,stacked ? 0xfffU : 0x1000U},{r::STATE_ST1,masked ? 0x800U : 0U,0x800}});
			if (pending && tick > 9) state.expected.push_back({r::STATE_IFR,tick < acceptance + 3 ? 1U : 0U});
			c.states.push_back({tick,std::move(state)});
		}
		c.require_idle = false; c.check_pipeline_clock = true;
		c.actions = {{2,{},false,{},{{0,ASSERT_LINE},{0,CLEAR_LINE}}}};
		if (pending) c.actions.push_back({9,{},false,{},{{0,ASSERT_LINE},{0,CLEAR_LINE}}});
		c.irqs = {{0,6,6,0,0x601}};
		c.program_events = {{false,base + 0x601,0xe844,3},{false,base + 0x40,opcode,6}};
		if (multiword) c.program_events.push_back({false,base + 0x42,0x1234,8});
		c.program_events.push_back({false,base + 0x601,0xe844,9});
		c.data_events = std::vector<clocked_data_event>{{true,0xfff,0x601,7}};
		if (pending) {
			u16 const continuation = delayed ? 0x604 : 0x602;
			c.irqs.push_back({0,acceptance + 3,acceptance + 3,1,continuation});
			c.data_events->push_back({true,0xfff,continuation,acceptance + 4});
			c.program_events.push_back({false,base + 0x40,opcode,acceptance + 3});
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
