// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u16 ops[]{0x6d96,0xf020,0xf272,0xf073,0x891a,0xf020,0x76f8,0x8186};
	for (unsigned page = 0; page < 2; ++page) for (unsigned kind = 0; kind < 8; ++kind)
	{
		u32 const base = page * 0x20000;
		bool const two_word = kind == 1 || kind == 2 || kind == 3 || kind == 6;
		u16 const following = kind == 1 || kind == 5 ? 0x1234 : kind == 6 ? 0x5000 :
			kind == 3 ? 0x602 : kind == 2 ? 0x604 : 0xf4e1;
		auto initial = initial_case("block-endpoint", {0xf072,u16(two_word ? 0x603 : 0x602),
			ops[kind],following,u16(kind == 6 ? 0xbeef : 0xf4e1)});
		for (auto &word : initial.program) word.address += base;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},
			{r::STATE_ST0,0xe08},{r::STATE_BRC,3},{r::STATE_AR6,kind == 7 ? 0x1a : 0x5000}});
		if (kind >= 2) initial.expected.clear();
		auto final = observation("endpoint-result", {});
		if (kind < 2) {
			final.expected.insert(final.expected.end(), {{r::STATE_BRC,0},{r::STATE_ST1,0,0x8000},
				{r::STATE_AR6,kind ? 0x5000U : 0x5004U}});
			if (kind) final.expected.push_back({r::STATE_A,0x1234});
		} else final.expected.front().value = 1;
		clocked_register_case c{{{0,std::move(initial)},{32,std::move(final)}},2,32};
		c.require_idle = kind < 2;
		// Nested control/count changes and malformed endpoints fail closed in
		// the staged implementation; these are not silicon-illegal claims.
		c.data_events = std::vector<clocked_data_event>{};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
