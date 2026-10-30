// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned predicate = 0; predicate < 2; ++predicate) for (unsigned tc = 0; tc < 2; ++tc)
	for (unsigned trn = 0; trn < 2; ++trn) {
		unsigned const seed = predicate + tc + trn;
		u32 const base = seed & 1 ? 0x20000 : 0;
		auto initial = initial_case("nondelayed-BC-ordinary-word-store", {u16(predicate ? 0xf830 : 0xf820),0x602,
			u16(trn ? 0x8df8 : 0x8cf8),0x1200,0xf4e1});
		for (auto &entry : initial.program) entry.address += base;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,0xa608U | (tc << 12)},
			{r::STATE_ST1,0x900},{r::STATE_T,0x2468},{r::STATE_TRN,0x3579}});
		auto final = observation("one-ordinary-store-on-either-branch-outcome", {{r::STATE_ST0,0xa608U | (tc << 12)},
			{r::STATE_ST1,0x900},{r::STATE_T,0x2468},{r::STATE_TRN,0x3579},{STATE_GENPC,base + 0x605}});
		clocked_register_case c{{{0,std::move(initial)},{24,std::move(final)}},seed & 1 ? 5U : 10U,24};
		c.data_events = std::vector<clocked_data_event>{{true,0x1200,u16(trn ? 0x3579 : 0x2468),0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
