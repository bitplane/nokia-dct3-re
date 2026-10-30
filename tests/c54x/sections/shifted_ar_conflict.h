// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned reg = 0; reg < 8; ++reg) for (unsigned kind = 0; kind < 4; ++kind)
	for (unsigned same = 0; same < 2; ++same) for (unsigned conflict = 0; conflict < 2; ++conflict) {
		unsigned const target = same ? reg : reg ^ 1, seed = reg + kind + same + conflict;
		u16 const destination = 0x10 + target, older = conflict ? 0x10 + reg : 0x63;
		u32 const base = seed & 1 ? 0x20000 : 0;
		auto initial = initial_case("shifted-store-DAGEN-bank", {u16(0x6f00 | older),0x0c81,
			u16(kind == 0 ? 0x7700 | destination : kind == 1 ? 0x7162 : kind == 2 ? 0x8a00 | destination : 0x7600 | destination),
			u16(kind == 1 ? destination : kind == 2 ? 0xf4e1 : 0xbeef),0xf4e1});
		for (auto &word : initial.program) word.address += base;
		initial.data = {{0x62,0xbeef},{0x1000,0xbeef}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_A,0x1234},{r::STATE_B,0xabcdef},
			{r::STATE_ST0,0},{r::STATE_ST1,0x900}});
		for (unsigned ar = 0; ar < 8; ++ar) initial.initial.push_back({r::STATE_AR0 + ar,0x4000 + ar});
		auto final = observation("owned-publications-only", {{r::STATE_A,0x1234},{r::STATE_B,0xabcdef},
			{r::STATE_ST0,0},{r::STATE_ST1,0x900},{r::STATE_SP,0x1000 + unsigned(kind == 2)}});
		for (unsigned ar = 0; ar < 8; ++ar) final.expected.push_back({int(r::STATE_AR0 + ar),ar == target ?
			0xbeefU : conflict && ar == reg ? 0x2468U : 0x4000U + ar});
		clocked_register_case c{{{0,std::move(initial)},{20,std::move(final)}},6 + seed % 3,20};
		c.data_events = std::vector<clocked_data_event>{};
		if (kind == 1 || kind == 2) c.data_events->push_back({false,kind == 1 ? 0x62U : 0x1000U,0xbeef,7});
		// Early mapped publication precedes older Write when the bank is free.
		if (!conflict) c.data_events->push_back({true,destination,0xbeef,7});
		c.data_events->push_back({true,older,0x2468,7});
		if (conflict) c.data_events->push_back({true,destination,0xbeef,8});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
