// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned source = 0; source < 2; ++source) for (u16 value : {0U,3U,4U,13U})
	for (unsigned gap = 0; gap < 4; ++gap) for (unsigned xmem = 0; xmem < 2; ++xmem) {
		unsigned const seed = source + value + gap + xmem;
		u16 const sampled = gap < 3 ? 8 : value, pointer = 0x5000 + (sampled ? sampled - 1 : 7);
		u32 const base = seed & 1 ? 0x20000 : 0;
		auto initial = initial_case("STLM-BK-circular-consumer", {u16(0x8819 | (source << 8))});
		initial.program[1 + gap].value = (xmem ? 0x94c0 : 0x12d2) | (source ? 0 : 0x100);
		initial.program[2 + gap].value = 0xf4e1;
		for (auto &word : initial.program) word.address += base;
		initial.data = {{pointer,0xcafe}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_A,source ? 0x123456789aULL : value},
			{r::STATE_B,source ? value : 0xfedcba9876ULL},{r::STATE_BK,8},{r::STATE_AR2,pointer},{r::STATE_AR0,1},
			{r::STATE_ST0,0xa608},{r::STATE_ST1,0x900}});
		u64 const loaded = xmem ? 0xffffffcafeULL : 0xcafe;
		clocked_register_case c{{{0,std::move(initial)},
			{5 + gap,observation("Decode-owned-BK", {{r::STATE_AR2,gap == 3 && !value ? 0x5008U : 0x5000U}})},
			{18,observation("single-publication-and-owned-wrap", {{r::STATE_A,source ? loaded : value},{r::STATE_B,source ? value : loaded},
				{r::STATE_BK,gap < 2 ? unsigned(value) : 5U},{r::STATE_AR2,gap == 3 && !value ? 0x5008U : 0x5000U},
				{r::STATE_ST0,0xa608},{r::STATE_ST1,0x900},{STATE_GENPC,base + 0x603 + gap}})}},
			std::array<unsigned,3>{3,4 + gap,5 + gap}[seed % 3],18};
		c.actions = {{4 + gap,{{r::STATE_BK,5}}}};
		c.data_events = std::vector<clocked_data_event>{};
		if (!gap) c.data_events->push_back({false,pointer,0xcafe,6});
		c.data_events->push_back({true,0x19,value,6});
		if (gap) c.data_events->push_back({false,pointer,0xcafe,6 + gap});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
