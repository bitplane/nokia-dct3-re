// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned target = 0; target < 2; ++target)
	for (unsigned count_index = 0; count_index < 4; ++count_index)
	for (unsigned flags0 = 0; flags0 < 4; ++flags0)
	for (unsigned flags1 = 0; flags1 < 4; ++flags1)
	{
		unsigned const count = std::array<unsigned,4>{0,1,3,17}[count_index];
		unsigned const seed = target + count_index + flags0 + flags1;
		u32 const base = seed & 1 ? 0x20000 : 0;
		u16 const st0 = 0xa024 | (flags0 << 11), st1 = 0x800 | (flags1 << 8);
		auto initial = initial_case("repeat-zero", {u16(0xf071 | (target << 8)),u16(count),0x7693,0x2468,0xf4e1});
		for (auto &word : initial.program) word.address += base + 0x8a00;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x9000},{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_A,0xff12345678ULL},{r::STATE_B,0x81abcdef01ULL},{r::STATE_AR3,0x1200}});
		clocked_register_case c{{{0,std::move(initial)}},std::array<unsigned,3>{3,5,7}[seed % 3],64};
		c.states.push_back({1,observation("before-clear", {{r::STATE_A,0xff12345678ULL},{r::STATE_B,0x81abcdef01ULL},
			{r::STATE_ST0,st0},{r::STATE_ST1,st1}})});
		auto cleared = observation("selected-accumulator-cleared", {{r::STATE_A,target ? 0xff12345678ULL : 0},
			{r::STATE_B,target ? 0 : 0x81abcdef01ULL},{r::STATE_ST0,st0},{r::STATE_ST1,st1}});
		c.states.push_back({7,cleared});
		cleared.expected.insert(cleared.expected.end(), {{r::STATE_RC,0},{r::STATE_AR3,0x1200 + count + 1}});
		c.states.push_back({64,std::move(cleared)});
		c.actions = {{5,{},false,{{base + 0x9001,0xffff}}}};
		c.program_events = {{false,base + 0x9001,u16(count),0,false}};
		c.data_events = std::vector<clocked_data_event>{};
		for (unsigned n = 0; n <= count; ++n) c.data_events->push_back({true,0x1200 + n,0x2468,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
