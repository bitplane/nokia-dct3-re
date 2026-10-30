// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (u16 count : {0U,1U,2U,0xffffU}) for (unsigned point : {6U,8U,9U})
	{
		u32 const base = count & 1 ? 0x20000 : 0;
		auto initial = initial_case("signed-branch-producer", {0x4811,0x08e6,0x11,0xf846,0x700,0xe911,0xf4e1});
		for (auto &word : initial.program) word.address += base;
		initial.program.insert(initial.program.end(), {{base + 0x700,0xe955},{base + 0x701,0xf4e1}});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xa608},
			{r::STATE_ST1,0x800},{r::STATE_AR1,count},{r::STATE_AR6,0x1000}});
		initial.data = {{0x1011,1}};
		auto final = observation("selected", {{r::STATE_A,count ? u64(count - 1) : 0xffffffffffULL},
			{r::STATE_B,count > 1 ? 0x55U : 0x11U},{r::STATE_ST0,count ? 0xae08U : 0xa608U},
			{r::STATE_AR6,0x1000}});
		clocked_register_case c{{{0,std::move(initial)},{30,std::move(final)}},point,30};
		c.data_events = std::vector<clocked_data_event>{{false,0x11,count,5},{false,0x1011,1,7}};
		if (count > 1) c.program_events = {{false,base + 0x700,0xe955,10}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
