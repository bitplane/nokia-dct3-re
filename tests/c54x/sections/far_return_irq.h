// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned variant = 0; variant < 8; ++variant)
	{
		bool const delayed = variant & 2, masked = variant & 4;
		u32 const base = variant & 1 ? 0x10000 : 0, target = (base ^ 0x10000) + 0x700;
		u16 const continuation = delayed ? 0x702 : 0x700;
		auto initial = initial_case("far-return-irq", {u16((delayed ? 0xf6e4 : 0xf4e4) | (masked ? 0 : 1))});
		for (auto &word : initial.program) word.address += base;
		for (unsigned i = 0; i < 32; ++i) {
			initial.program.push_back({target + i,u16(i ? 0xf495 : 0xe811)});
			initial.program.push_back({0x44 + i,u16(i ? 0xf495 : 0xe955)});
			initial.program.push_back({0x10044 + i,u16(i ? 0xf495 : 0xe955)});
		}
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_IMR,2}});
		initial.data = {{0x1000,u16(target >> 16)},{0x1001,0x700}};
		clocked_register_case c{{{0,std::move(initial)}},5,20};
		c.states.push_back({20,observation("restored-page-interrupt", {
			{r::STATE_SP,masked ? 0x1002U : 0x1001U},{r::STATE_A,masked || delayed ? 0x11U : 0U},
			{r::STATE_B,masked ? 0U : 0x55U},{r::STATE_IFR,masked ? 2U : 0U}})});
		c.actions = {{4,{{r::STATE_IFR,2}}}};
		c.require_idle = false; c.check_pipeline_clock = true;
		unsigned const read = delayed ? 4 : 5;
		c.data_events = std::vector<clocked_data_event>{{false,0x1000,u16(target >> 16),read},{false,0x1001,0x700,read + 1}};
		if (!masked) {
			c.program_events = {{false,(target & 0x10000) + 0x44,0xe955,0,false}};
			c.data_events->push_back({true,0x1001,continuation,0,false});
			c.irqs = {{1,5,20,2,continuation}};
			c.states.back().observation.expected_data.push_back({0x1001,continuation});
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
