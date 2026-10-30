// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned b = 0; b < 2; ++b) for (unsigned zero = 0; zero < 2; ++zero) for (unsigned gap = 0; gap < 2; ++gap) {
		unsigned const seed = b + 2 * zero + 4 * gap;
		u32 const base = seed & 1 ? 0x20000 : 0;
		auto initial = initial_case("load-to-conditional-return", {u16(0x1292 | (b << 8))});
		initial.program[1 + gap].value = b ? 0xfc4d : 0xfc45;
		initial.program[2 + gap].value = 0xf4e1;
		for (auto &entry : initial.program) entry.address += base;
		initial.program.push_back({base + 0x700,0xf4e1});
		initial.data = {{0x1200,u16(!zero)},{0x1000,0x700}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x800},
			{r::STATE_SP,0x1000},{r::STATE_AR2,0x1200},{r::STATE_A,b ? 0x1234U : zero},{r::STATE_B,b ? zero : 0x2345U}});
		auto final = observation("return-predicate-uses-loaded-not-old-value", {{STATE_GENPC,base + (zero ? 0x701 : 0x603 + gap)},
			{r::STATE_SP,0x1000 + zero},{r::STATE_AR2,0x1201},{r::STATE_A,b ? 0x1234U : !zero},{r::STATE_B,b ? !zero : 0x2345U}});
		clocked_register_case c{{{0,std::move(initial)},{24,std::move(final)}},4 + seed % 4,24};
		c.data_events = std::vector<clocked_data_event>{{false,0x1200,u16(!zero),0,false}};
		if (zero) c.data_events->push_back({false,0x1000,0x700,0,false});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
