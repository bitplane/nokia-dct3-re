// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned operation = 0; operation < 3; ++operation) for (unsigned offset : {0U,0x20U})
	for (u16 before : {0U,1U,0x8000U,0xffffU}) for (u16 immediate : {0xffU,0xffffU})
	for (unsigned compiler = 0; compiler < 2; ++compiler)
	{
		u16 const address = (compiler ? 0x1000 : 0x400) + offset;
		u16 const result = operation == 0 ? before & immediate : operation == 1 ? before | immediate : u16(before + immediate);
		u32 const base = (operation + offset / 32 + before + compiler) % 2 ? 0x20000 : 0;
		auto initial = initial_case("direct-memory-modify", {u16(std::array<u16,3>{0x6800,0x6900,0x6b00}[operation] | offset),immediate});
		for (auto &word : initial.program) word.address += base;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xa608},
			{r::STATE_ST1,compiler ? 0x4900 : 0x900},{r::STATE_SP,0x1000}});
		initial.data = {{address,before}};
		auto final = observation("retained-write", {{r::STATE_SP,compiler ? 0x1020U : 0x1000U},
			{r::STATE_ST0,compiler ? 8U : 9U,0x1ff},{r::STATE_ST1,compiler ? 0x900U : 0x4900U}}, {{address,result}});
		clocked_register_case c{{{0,std::move(initial)},{7,std::move(final)}},
			4 + (operation + offset / 32 + before + immediate + compiler) % 4,20};
		c.require_idle = false;
		c.actions = {{4,{{r::STATE_ST1,compiler ? 0x900 : 0x4900}},false,{{base + 0x601,0xdead}}},
			{5,{{compiler ? r::STATE_SP : r::STATE_ST0,compiler ? 0x1020 : 0xa609}}},{6,{}}};
		c.actions.back().data = {{address,0x5555}};
		c.program_events = {{false,base + 0x601,immediate,0,false}};
		c.data_events = std::vector<clocked_data_event>{{false,address,before,6},{true,address,result,7}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
