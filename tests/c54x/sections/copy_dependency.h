// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned pattern = 0; pattern < 4; ++pattern)
	for (unsigned modifier = 0; modifier < 4; ++modifier)
	for (unsigned consumer = 0; consumer < 2; ++consumer)
	for (unsigned gap = 0; gap < 2; ++gap)
	for (unsigned layout = 0; layout < 3; ++layout)
	{
		u16 const word = std::array<u16,4>{0,1,0x8000,0xffff}[pattern];
		unsigned const seed = pattern + modifier + consumer + gap + layout;
		u32 const base = seed & 1 ? 0x20000 : 0;
		unsigned const at = (layout ? 0x700 : 0x601) + gap, read = layout ? 6 + layout : 5;
		auto initial = initial_case("copy-dependency", {0xf495});
		for (auto &entry : initial.program) entry.address += base;
		for (unsigned i = 0; i < 24; ++i) initial.program.push_back({base + 0x700 + i,0xf495});
		auto put = [&](unsigned address, u16 value) {
			for (auto &entry : initial.program) if (entry.address == base + address) { entry.value = value; return; }
			initial.program.push_back({base + address,value});
		};
		if (layout) { put(0x600,0xf274); put(0x601,0x700); put(0x604,0xf4e1); }
		put(layout ? 0x601 + layout : 0x600,0xe521 | (modifier << 6));
		put(at,consumer ? 0x9410 : 0x1083); put(at + 1,0x80f8); put(at + 2,0x4100); put(at + 3,layout ? 0xfc00 : 0xf4e1);
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0x1800},{r::STATE_ST1,0x900},
			{r::STATE_SP,0x3000},{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfeabcd9876ULL},{r::STATE_T,0x5aa5},
			{r::STATE_AR0,3},{r::STATE_BK,0},{r::STATE_AR3,0x4000},{r::STATE_AR4,0x5000}});
		initial.data = {{0x4000,0x2468},{0x4100,0xdead},{0x5000,word}};
		clocked_register_case c{{{0,std::move(initial)}},std::array<unsigned,4>{3,5,6,8}[seed % 4],40};
		c.states.push_back({40,observation("copied-value-consumed", {{STATE_GENPC,base + (layout ? 0x605 : at + 4)},
			{r::STATE_SP,0x3000},{r::STATE_A,u64(s64(s16(word))) & 0xffffffffffULL},{r::STATE_B,0xfeabcd9876ULL},
			{r::STATE_T,0x5aa5},{r::STATE_ST0,0x1800},{r::STATE_ST1,0x900},{r::STATE_AR3,0x4000},
			{r::STATE_AR4,unsigned(0x5000 + (modifier == 1 ? -1 : modifier == 2 ? 1 : modifier == 3 ? 3 : 0))}},
			{{0x4000,word},{0x4100,word}})});
		c.actions = {{read,{},false,{}, {},{{0x5000,u16(~word)}}}};
		c.data_events = std::vector<clocked_data_event>{};
		if (layout) c.data_events->push_back({true,0x2fff,0x604,6});
		c.data_events->push_back({false,0x5000,word,read});
		c.data_events->push_back({true,0x4000,word,read + 1});
		if (gap || layout == 1) c.data_events->push_back({false,0x4000,word,0,false});
		if (layout) c.data_events->push_back({false,0x2fff,0x604,0,false});
		c.data_events->push_back({true,0x4100,word,0,false});
		c.clear_pipeline_before_restore = true;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
