// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned prefix = 0; prefix < 3; ++prefix) for (unsigned count : {0U,1U,2U,7U,127U}) {
		if (!prefix && count == 127) continue;
		unsigned const seed = prefix + count, finish = prefix ? 32 + count * 5 : 96;
		u32 const base = seed & 1 ? 0x20000 : 0;
		auto initial = initial_case("status-or-counter-publication-before-RPTB",
			{0x730b,7,0xf072,0x608,0x1386,0xf678,0xf3e8,0xf1a0,0x8196,0xf4e1});
		if (prefix) { initial.program[0].value = prefix == 2 ? 0x8a1a : 0x771a; initial.program[1].value = prefix == 2 ? 0xf495 : count; }
		for (auto &entry : initial.program) entry.address += base;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0},{r::STATE_ST1,prefix ? 0x800U : 0x900U},
			{r::STATE_SP,0x1000},{r::STATE_A,0},{r::STATE_B,0x800},{r::STATE_AR6,0x5000},{r::STATE_BRC,prefix ? count ^ 1 : count}});
		if (prefix == 2) initial.data.push_back({0x1000,u16(count)});
		for (unsigned i = 0; i <= count; ++i) initial.data.push_back({0x5000 + i,u16(0x1234 + i * 0x321)});
		auto final = observation("count-plus-one-swaps-and-cleared-BRAF", {{STATE_GENPC,base + 0x60a},{r::STATE_AR6,0x5001 + count},
			{r::STATE_BRC,0},{r::STATE_ST1,0x800},{r::STATE_SP,prefix == 2 ? 0x1001U : 0x1000U}});
		for (unsigned i = 0; i <= count; ++i) {
			u16 const word = 0x1234 + i * 0x321;
			final.expected_data.push_back({0x5000 + i,u16((word << 8) | (word >> 8))});
		}
		clocked_register_case c{{{0,std::move(initial)},
			{6,observation("younger-BRAF-survives-older-status-write", {{r::STATE_ST1,0x8800}})},
			{7,observation("block-running", {})},{finish,std::move(final)}},
			prefix ? std::array<unsigned,3>{4,5,6}[seed % 3] : std::array<unsigned,3>{5,6,12}[seed % 3],finish};
		if (prefix == 1) {
			c.actions = {{4,{},false,{{base + 0x601,0xdead}}}};
			c.program_events = {{false,base + 0x601,u16(count),0,false}};
		} else if (prefix == 2) c.actions = {{5,{},false,{},{},{{0x1000,u16(count ^ 0xffff)}}}};
		c.data_events = std::vector<clocked_data_event>{};
		if (!prefix) {
			c.data_events->push_back({false,0xb,0x800,5});
			c.data_events->push_back({true,7,0x800,6});
		} else {
			if (prefix == 2) c.data_events->push_back({false,0x1000,u16(count),5});
			c.data_events->push_back({true,0x1a,u16(count),prefix == 2 ? 6U : 5U});
		}
		for (unsigned i = 0; i <= count; ++i) {
			c.data_events->push_back({false,0x5000 + i,u16(0x1234 + i * 0x321),0,false});
			if (i) {
				u16 const previous = 0x1234 + (i - 1) * 0x321;
				c.data_events->push_back({true,0x5000 + i - 1,u16((previous << 8) | (previous >> 8)),0,false});
			}
		}
		u16 const last = 0x1234 + count * 0x321;
		c.data_events->push_back({true,0x5000 + count,u16((last << 8) | (last >> 8)),0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
