// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned operation = 0; operation < 4; ++operation)
	for (unsigned span = 0; span < 2; ++span)
	for (unsigned page = 0; page < 2; ++page)
	{
		bool const subtract = operation >= 2, b = operation & 1;
		u32 const base = page ? 0x20000 : 0;
		u16 const source = subtract ? 0x1011 : 0x1000;
		u64 const result = subtract ? 0x1233 : 0x1235;
		auto initial = initial_case("xc-arithmetic", {u16(span ? 0xff00 : 0xfd00),
			std::array<u16,4>{0x00f8,0x01f8,0x08e1,0x09e1}[operation],
			u16(subtract ? 0x11 : source),u16(b ? 0xe866 : 0xe966),0xf495,0xf495,0xf4e1});
		for (auto &word : initial.program) word.address += base;
		initial.program.insert(initial.program.end(), {{base + 0x40,0x4a1e},{base + 0x41,0xf495},{base + 0x42,0xf4e5}});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xa608},
			{r::STATE_ST1,0},{r::STATE_AR1,0x1000},{r::STATE_A,b ? 0U : 0x1234U},
			{r::STATE_B,b ? 0x1234U : 0U},{r::STATE_IMR,1}});
		initial.data = {{source,1}};
		clocked_register_case c{{{0,std::move(initial)}},(operation + span + page) & 1 ? 8U : 4U,36};
		c.states.push_back({4,observation("pending-source-latched", {{r::STATE_IFR,1}})});
		c.states.push_back({8,observation("protected-result", {{int(b ? r::STATE_B : r::STATE_A),result}})});
		c.states.push_back({36,observation("returned-to-caller", {{r::STATE_SP,0x1000},
			{r::STATE_A,b ? 0x66 : result},{r::STATE_B,b ? result : 0x66}})});
		c.actions = {{3,{},false,{},{{0,ASSERT_LINE},{0,CLEAR_LINE}}}};
		c.data_events = std::vector<clocked_data_event>{{false,source,1,7},
			{true,0xfff,0x606,0,false},{false,0x1e,u16(base >> 16),0,false},
			{true,0xffe,u16(base >> 16),0,false},{false,0xffe,u16(base >> 16),0,false},
			{false,0xfff,0x606,0,false}};
		c.program_events = {{false,base + 0x40,0x4a1e,11}};
		c.irqs = {{0,9,36,1}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
