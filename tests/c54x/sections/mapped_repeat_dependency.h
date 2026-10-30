// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 32; ++index) {
		unsigned const count = std::array<unsigned,4>{0,1,3,7}[index % 4];
		bool const subtract = index & 4, call = index & 8, delayed = index & 16;
		u32 const base = (index + index / 4) & 1 ? 0x20000 : 0;
		u32 const setup = base + (call ? 0x700 : 0x604), body = setup + (delayed ? 4 : 2);
		u16 const before = subtract ? count + 1 : count - 1;
		auto initial = initial_case("mapped-repeat-dependency", {0x6bf8,0x1a,u16(subtract ? 0xffff : 1)});
		for (auto &word : initial.program) word.address += base;
		// Initialize both complete regions: no callee inherits a previous case.
		for (unsigned i = 0; i < 32; ++i) {
			if (i >= 3) initial.program.push_back({base + 0x600 + i,0xf495});
			initial.program.push_back({base + 0x700 + i,0xf495});
		}
		auto put = [&](u32 at,u16 value) { for (auto &word : initial.program) if (word.address == at) word.value = value; };
		if (call) { put(base + 0x603,0xf074); put(base + 0x604,0x700); put(base + 0x605,0xf4e1); }
		put(setup,delayed ? 0xf272 : 0xf072); put(setup + 1,u16(body + 1)); put(body,0x6d90); put(body + 2,call ? 0xfc00 : 0xf4e1);
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_BRC,before},
			{r::STATE_ST0,0xa024},{r::STATE_ST1,0x900},{r::STATE_SP,0x1000},{r::STATE_AR0,0x1200}});
		clocked_register_case c{{{0,std::move(initial)}},std::array<unsigned,5>{6,7,8,10,16}[index % 5],128};
		c.states.push_back({128,observation("published-count-controls-iterations", {{STATE_GENPC,call ? base + 0x606 : body + 3},
			{r::STATE_AR0,0x1201 + count},{r::STATE_BRC,0},{r::STATE_ST1,0x900},{r::STATE_SP,0x1000}})});
		c.data_events = std::vector<clocked_data_event>{{false,0x1a,before,0,false},{true,0x1a,u16(count),0,false}};
		if (call) {
			c.data_events->push_back({true,0xfff,0x605,0,false});
			c.data_events->push_back({false,0xfff,0x605,0,false});
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
