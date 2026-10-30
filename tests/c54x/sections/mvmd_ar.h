// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned reg = 0; reg < 8; ++reg) for (unsigned kind = 0; kind < 3; ++kind)
	for (u16 word : {0U,1U,0x8000U,0xffffU}) {
		unsigned const seed = reg + kind + word;
		u16 const source = 0x10 + reg, destination = kind == 0 ? 0x1400 : kind == 1 ? source : 0x10 + (reg + 1) % 8;
		u32 const base = seed & 1 ? 0x20000 : 0;
		auto initial = initial_case("MVMD-AR-retained-source", {u16(0x7310 | reg),destination,0xf4e1});
		for (auto &entry : initial.program) entry.address += base;
		initial.data = {{destination,0xaaaa}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{int(r::STATE_AR0 + reg),word},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfedcba9876ULL},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x800}});
		auto final = observation("owned-RAM-or-mapped-publication", {{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfedcba9876ULL},
			{r::STATE_ST0,0xa608},{r::STATE_ST1,0x800},{STATE_GENPC,base + 0x603}},
			{{destination,word},{source,source == destination ? word : u16(0xdead)}});
		clocked_register_case c{{{0,std::move(initial)},{12,std::move(final)}},std::array<unsigned,3>{4,5,7}[seed % 3],12};
		c.actions = {{4,{},false,{{base + 0x601,0xdead}}}};
		if (source != destination) c.actions.push_back({5,{},false,{},{},{{source,0xdead}}});
		c.program_events = {{false,base + 0x601,destination,0,false}};
		c.data_events = std::vector<clocked_data_event>{{false,source,word,5},{true,destination,word,kind ? 5U : 6U}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
