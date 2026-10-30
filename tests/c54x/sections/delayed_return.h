// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned form = 0; form < 4; ++form)
	for (unsigned page = 0; page < 2; ++page)
	{
		bool const xc = form >= 2, multiword = form == 1, irq = form == 2;
		u32 const base = page ? 0x10000 : 0;
		auto initial = initial_case("delayed-return", {0xfe00,u16(xc ? 0xfd45 : multiword ? 0x770e : 0xe811),
			u16(xc ? 0xf6bb : multiword ? 6 : 0xe944),0xffff});
		for (auto &word : initial.program) word.address += base;
		for (unsigned i = 0; i < 24; ++i) {
			initial.program.push_back({base + 0x700 + i,u16(i ? 0xf495 : 0xe955)});
			initial.program.push_back({base + 0x40 + i,0xf495});
		}
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xa608},
			{r::STATE_A,form == 3 ? 0x800U : 0U},{r::STATE_T,0},{r::STATE_IMR,xc ? 1U : 0U}});
		initial.data = {{0x1000,0x700}};
		unsigned const point = (xc ? 6U : 4U) + (form + page) % 3;
		clocked_register_case c{{{0,std::move(initial)}},point,15};
		if (xc) c.actions = {{6,{{r::STATE_IFR,1}},true}};
		for (unsigned tick = 1; tick <= 15; ++tick) {
			auto state = observation("return-delays-and-mask", {
				{r::STATE_SP,irq && tick >= 13 ? 0x1000U : tick >= 4 ? 0x1001U : 0x1000U},
				{r::STATE_ST1,irq && tick >= 10 && tick < 14 ? 0U : 0x800U},
				{r::STATE_A,xc ? form == 3 ? 0x800U : 0U : !multiword && tick >= 9 ? 0x11U : 0U},
				{r::STATE_B,tick >= 11 ? 0x55U : !xc && !multiword && tick >= 10 ? 0x44U : 0U},
				{r::STATE_T,multiword && tick >= 8 ? 6U : 0U}});
			if (xc) state.expected.push_back({r::STATE_IFR,irq && tick >= 14 ? 0U : tick >= 6 ? 1U : 0U});
			c.states.push_back({tick,std::move(state)});
		}
		c.require_idle = false; c.check_pipeline_clock = true;
		c.program_events = {{false,base + 0x601,u16(xc ? 0xfd45 : multiword ? 0x770e : 0xe811),3},
			{false,base + 0x602,u16(xc ? 0xf6bb : multiword ? 6 : 0xe944),4},{false,base + 0x700,0xe955,7}};
		c.data_events = std::vector<clocked_data_event>{{false,0x1000,0x700,5}};
		if (multiword) c.data_events->push_back({true,0xe,6,8});
		if (irq) {
			c.irqs = {{0,14,14,1}};
			c.data_events->push_back({true,0x1000,0x704,15});
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
