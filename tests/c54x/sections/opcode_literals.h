// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (u16 destination : {0x7cf8U,0x7df8U,0x7ff8U}) for (unsigned count = 0; count < 3; ++count)
	{
		u32 const base = count & 1 ? 0x20000 : 0;
		auto initial = initial_case("opcode-shaped-extension", {0xf072,0x604,0x76f8,destination,0x1357,0xf4e1});
		for (auto &word : initial.program) word.address += base;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},
			{r::STATE_ST0,0xa624},{r::STATE_ST1,0x800},{r::STATE_BRC,count}});
		initial.data = {{destination,0xbeef}};
		auto final = observation("literal-destination", {}, {{destination,0x1357}});
		clocked_register_case c{{{0,std::move(initial)},{64,std::move(final)}},
			std::array<unsigned,3>{4,7,12}[(destination / 256 + count) % 3],64};
		c.data_events = std::vector<clocked_data_event>(count + 1,{true,destination,0x1357,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
