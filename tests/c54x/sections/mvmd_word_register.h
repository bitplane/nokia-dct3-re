// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned context = 0; context < 4; ++context) for (unsigned target = 0; target < 9; ++target)
	for (u16 word : {0U,1U,0x8000U,0xffffU}) {
		bool const producer = context == 3, bk = context >= 2;
		unsigned const prefix = producer ? 2 : 0, seed = context + target + word;
		u16 const source = bk ? 0x19 : 0xe + context, destination = target < 8 ? 0x10 + target : 0x19;
		u32 const base = seed & 1 ? 0x20000 : 0;
		auto initial = initial_case("MVMD-word-register", {0xf495});
		if (producer) initial.program[0].value = 0x8819;
		initial.program[prefix].value = 0x7300 | source;
		initial.program[prefix + 1].value = destination; initial.program[prefix + 2].value = 0xf4e1;
		for (auto &entry : initial.program) entry.address += base;
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_A,producer ? word : 0x123456789aULL},
			{r::STATE_B,0xfedcba9876ULL},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x900},
			{r::STATE_T,context == 0 ? unsigned(word) : 0x1357U},{r::STATE_TRN,context == 1 ? unsigned(word) : 0x2468U},
			{r::STATE_BK,bk && !producer ? unsigned(word) : 13U}});
		for (unsigned ar = 0; ar < 8; ++ar) initial.initial.push_back({r::STATE_AR0 + ar,0x1400 + ar});
		auto final = observation("captured-word-not-replaced-or-republished", {{r::STATE_A,producer ? word : 0x123456789aULL},
			{r::STATE_B,0xfedcba9876ULL},{r::STATE_SP,0x1000},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x900},
			{r::STATE_T,context == 0 ? 0xdeadU : 0x1357U},{r::STATE_TRN,context == 1 ? 0xdeadU : 0x2468U},
			{r::STATE_BK,bk ? 0xdeadU : target == 8 ? unsigned(word) : 13U},{STATE_GENPC,base + 0x603 + prefix}});
		for (unsigned ar = 0; ar < 8; ++ar) final.expected.push_back({int(r::STATE_AR0 + ar),ar == target ? unsigned(word) : 0x1400U + ar});
		clocked_register_case c{{{0,std::move(initial)},{12,std::move(final)}},4 + prefix + seed % 3,12};
		c.actions = {{5 + prefix,{},false,{},{},{{source,0xdead}}}};
		c.data_events = std::vector<clocked_data_event>{};
		if (producer) c.data_events->push_back({true,0x19,word,6});
		c.data_events->push_back({false,source,word,5 + prefix});
		c.data_events->push_back({true,destination,word,5 + prefix});
		cases.push_back(std::move(c));
	}
	return cases;
}
}
