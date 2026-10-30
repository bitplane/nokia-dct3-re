// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 64; ++index) {
		u16 const value = std::array<u16,4>{0,1,0x7f,0x8000}[index % 4], literal = index & 16 ? 0x7f : 1;
		bool const tc = index & 4, old_tc = index & 8, cpl = index & 32, result = value == literal, taken = result == tc;
		u16 const address = cpl ? 0x4014 : 0x1694;
		u32 const base = (index + index / 4) & 1 ? 0x20000 : 0;
		u16 const st1 = cpl ? 0x4800 : 0x800;
		auto initial = initial_case("direct-memory-compare", {0x6014,literal,0xf495,u16(0xf820 | (tc ? 0x10 : 0)),0x700,0xe811,0xf4e1});
		for (auto &word : initial.program) word.address += base;
		initial.program.insert(initial.program.end(),{{base + 0x700,0xe855},{base + 0x701,0xf4e1}});
		initial.data = {{address,value}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,u64(0xa62d | (old_tc ? 0x1000 : 0))},
			{r::STATE_ST1,st1},{r::STATE_SP,0x4000},{r::STATE_B,0x5678}});
		clocked_register_case c{{{0,std::move(initial)}},std::array<unsigned,3>{3,6,7}[(index + index / 4) % 3],30};
		for (unsigned tick = 1; tick <= 7; ++tick) c.states.push_back({tick,observation("TC-publishes-at-Execute", {
			{r::STATE_ST0,u64((tick < 5 ? 0xa62d : 0xa633) | ((tick < 7 ? old_tc : result) ? 0x1000 : 0))}})});
		c.states.push_back({30,observation("retained-comparison-selects-branch", {{r::STATE_A,taken ? 0x55U : 0x11U},
			{r::STATE_B,0x5678},{r::STATE_ST0,u64(0xa633 | (result ? 0x1000 : 0))},{r::STATE_SP,0x6000},
			{r::STATE_ST1,st1},{STATE_GENPC,base + (taken ? 0x702 : 0x607)}})});
		c.actions = {{4,{},false,{{base + 0x601,0xdead}}},
			{5,{{r::STATE_ST0,u64(0xa633 | (old_tc ? 0x1000 : 0))},{r::STATE_SP,0x6000}},true},
			{6,{},false,{}, {},{{address,0x7777}}}};
		c.data_events = std::vector<clocked_data_event>{{false,address,value,6}};
		c.program_events = {{false,base + 0x601,literal,0,false}};
		if (taken) c.program_events.push_back({false,base + 0x700,0xe855,10});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
