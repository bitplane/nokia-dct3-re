// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned reg = 0; reg < 8; ++reg) for (unsigned kind = 0; kind < 3; ++kind)
	for (u16 word : {0U,1U,0x8000U,0xffffU}) for (unsigned gap = 0; gap < 2; ++gap) {
		unsigned const seed = reg + kind + word + gap;
		u16 const source = kind == 0 ? 0x500 : 0x10 + (kind == 1 ? reg : (reg + 1) % 8);
		u32 const base = seed & 1 ? 0x20000 : 0;
		auto initial = initial_case("MVDM-AR-compare-latency", {u16(0x7210 | reg),source,0xf495,0xf495,0xf4e1});
		initial.program[2 + gap].value = 0xf4a8 | reg;
		initial.program[3 + gap].value = 0xf4e1;
		for (auto &entry : initial.program) entry.address += base;
		std::array<u16,8> pointers;
		for (unsigned ar = 0; ar < 8; ++ar) pointers[ar] = 0x400 + ar;
		pointers[0] = word;
		if (kind) pointers[source - 0x10] = word;
		else initial.data = {{source,word}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x800}});
		for (unsigned ar = 0; ar < 8; ++ar) initial.initial.push_back({r::STATE_AR0 + ar,pointers[ar]});
		bool const equal = gap || kind == 1 || reg == 0;
		auto final = observation("one-gap-AR-consumer", {{r::STATE_ST0,0xa608U | (equal ? 0x1000U : 0U)},
			{r::STATE_ST1,0x800},{STATE_GENPC,base + 0x604 + gap}});
		for (unsigned ar = 0; ar < 8; ++ar) final.expected.push_back({int(r::STATE_AR0 + ar),ar == reg ? unsigned(word) : unsigned(pointers[ar])});
		clocked_register_case c{{{0,std::move(initial)},
			{6,observation("owned-mapped-publication", {{int(r::STATE_AR0 + reg),word}})},{12,std::move(final)}},
			std::array<unsigned,3>{5,6,8}[seed % 3],12};
		c.actions = {{4,{},false,{{base + 0x601,0xdead}}}};
		if (!kind) c.actions.push_back({6,{},false,{},{},{{source,0xdead}}});
		c.program_events = {{false,base + 0x601,source,0,false}};
		c.data_events = std::vector<clocked_data_event>{{false,source,word,6},{true,0x10 + reg,word,6}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
